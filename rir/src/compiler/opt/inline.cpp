#include "inline.h"
#include "../analysis/generic_static_analysis.h"
#include "../analysis/query.h"
#include "../pir/pir_impl.h"
#include "../transform/bb.h"
#include "../transform/replace.h"
#include "../util/cfg.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <algorithm>
#include <unordered_map>

namespace {

using namespace rir::pir;

/* We need to figure out which loads we can inline. Our simple inlining strategy
 * can only deal with situations where it is unambiguous that a promise was
 * forced. For example in the following case:
 *
 *      Branch
 *   /          \
 * LdArg(0)     |
 *   \         /
 *     \     /
 *        |
 *      LdArg(0)
 *
 * we don't know at the second LdArg if the promise was forced (by the left
 * branch)
 * or not, thus we don't know if we should inline the promise code. In those
 * situations
 * we need to keep the promises around.
 */

struct PromiseEvaluationPoint : public std::vector<Instruction*> {
    static Instruction* unevaluated() { return (Instruction*)0x11; }
    static Instruction* ambiguous() { return (Instruction*)0x22; }
    void evalAt(size_t id, Instruction* i) {
        if (at(id) == unevaluated())
            (*this)[id] = i;
    }
    bool merge(PromiseEvaluationPoint& other) {
        if (size() == 0 && other.size() != 0) {
            *this = other;
            return true;
        }

        bool changed = false;
        for (size_t p = 0; p < size(); ++p) {
            Instruction* i = at(p);
            if (i == ambiguous() || i == other[p])
                continue;
            (*this)[p] = ambiguous();
            changed = true;
        }
        return changed;
    }
};

class PromEvalAnalysis : public StaticAnalysis<PromiseEvaluationPoint> {
  public:
    size_t nargs;
    PromEvalAnalysis(size_t nargs, BB* bb)
        : StaticAnalysis<PromiseEvaluationPoint>(bb), nargs(nargs) {}

    void init(PromiseEvaluationPoint& p) {
        p.resize(nargs);
        for (size_t i = 0; i < nargs; ++i) {
            p[i] = PromiseEvaluationPoint::unevaluated();
        }
    }
    void apply(PromiseEvaluationPoint& p, Instruction* i) {
        LdArg* ld = LdArg::Cast(i);
        if (!ld)
            return;
        p.evalAt(ld->id, i);
    }
};

class TheInliner {
  public:
    Function* function;
    TheInliner(Function* function) : function(function) {}

    void operator()() {

        Visitor::run(function->entry, [&](BB* bb) {
            // Dangerous iterater usage, works since we do only update it in
            // one place.
            for (auto it = bb->begin(); it != bb->end(); it++) {
                Call* call = Call::Cast(*it);
                if (!call)
                    continue;
                MkClsFun* cls = MkClsFun::Cast(call->cls());
                if (!cls)
                    continue;
                Function* inlinee = cls->fun;
                if (inlinee->arg_name.size() != call->nCallArgs())
                    continue;
                bool needCalleeEnv = !Query::doesNotNeedEnv(inlinee);

                BB* split = BBTransform::split(++function->max_bb_id, bb, it);

                Call* newCall = Call::Cast(*split->begin());
                std::vector<MkArg*> arguments;
                for (size_t i = 0; i < newCall->nCallArgs(); ++i) {
                    MkArg* a = MkArg::Cast(newCall->callArgs()[i]);
                    assert(a);
                    arguments.push_back(a);
                }

                BB* copy =
                    BBTransform::clone(&function->max_bb_id, inlinee->entry);
                bb->next0 = copy;

                // Find evaluation dominance order of LdArgs
                PromEvalAnalysis promeval(inlinee->arg_name.size(), copy);
                promeval();

                std::unordered_map<Instruction*, Value*> promiseResult;
                Visitor::run(copy, [&](BB* bb) {
                    for (auto it = bb->begin(); it != bb->end(); it++) {
                        Instruction* i = *it;
                        LdArg* ld = LdArg::Cast(*it);
                        if (!ld)
                            continue;
                        MkArg* a = arguments[ld->id];
                        Value* strict = a->arg<0>();
                        Instruction* dominatingLoad =
                            promeval.exitpoint[ld->id];
                        if (strict != Missing::instance()) {
                            ld->replaceUsesWith(strict);
                            it = bb->remove(it);
                        } else if (dominatingLoad == i) {
                            Promise* prom = a->prom;
                            BB* split = BBTransform::split(
                                ++function->max_bb_id, bb, it);
                            BB* prom_copy = BBTransform::clone(
                                &function->max_bb_id, prom->entry);
                            bb->next0 = prom_copy;
                            Value* promRes =
                                BBTransform::forInline(prom_copy, split);
                            Replace::usesOfValue(prom_copy, prom->env,
                                                 a->env());
                            promiseResult[i] = promRes;
                            it = split->begin();
                            ld = LdArg::Cast(*it);
                            assert(ld);
                            ld->replaceUsesWith(promRes);
                            it = split->remove(it);
                            bb = split;
                        }
                        if (it == bb->end())
                            return;
                    }
                });

                std::unordered_map<size_t, size_t> copiedPromises;
                Visitor::run(copy, [&](BB* bb) {
                    for (auto it = bb->begin(); it != bb->end(); it++) {
                        LdArg* ld = LdArg::Cast(*it);
                        MkArg* mk = MkArg::Cast(*it);
                        if (ld) {
                            MkArg* a = arguments[ld->id];
                            Instruction* dominatingLoad =
                                promeval.exitpoint[ld->id];
                            if (promiseResult.count(dominatingLoad))
                                ld->replaceUsesWith(
                                    promiseResult.at(dominatingLoad));
                            else
                                ld->replaceUsesWith(a);
                            it = bb->remove(it);
                        } else if (mk) {
                            Promise* prom = mk->prom;
                            if (prom->fun == inlinee) {
                                if (copiedPromises.count(prom->id)) {
                                    mk->prom =
                                        function
                                            ->promise[copiedPromises[prom->id]];
                                } else {
                                    BB* promCopy =
                                        BBTransform::clone(prom->entry);
                                    Promise* clone =
                                        new Promise(prom->env, function,
                                                    function->promise.size());
                                    clone->id = function->promise.size();
                                    function->promise.push_back(clone);
                                    clone->entry = promCopy;
                                    copiedPromises[prom->id] = clone->id;
                                    mk->prom = clone;
                                }
                            }
                        }
                        if (it == bb->end())
                            return;
                    }
                });

                Value* inlineeRes = BBTransform::forInline(copy, split);
                newCall->replaceUsesWith(inlineeRes);
                if (needCalleeEnv) {
                    MkEnv* env = new MkEnv(cls->env(), inlinee->arg_name,
                                           newCall->callArgs());
                    copy->insert(copy->begin(), env);
                    Replace::usesOfValue(copy, inlinee->env, env);
                }
                // Remove the call instruction
                split->remove(split->begin());

                bb = split;
                it = split->begin();

                // Can happen if split only contained the call instruction
                if (it == split->end())
                    break;
            }
        });
    }
};
}

namespace rir {
namespace pir {

void Inline::apply(Function* function) {
    TheInliner s(function);
    s();
}
}
}
