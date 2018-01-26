#include "inline.h"
#include "../analysis/generic_static_analysis.h"
#include "../analysis/query.h"
#include "../pir/pir_impl.h"
#include "../transform/bb.h"
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
    std::unordered_map<Instruction*, Instruction*> loadedAt;

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
        loadedAt[i] = p[ld->id];
    }
};

class TheInliner {
  public:
    Function* function;
    TheInliner(Function* function) : function(function) {}

    void operator()() {

        Visitor::run(function->entry, [&](BB* bb) {
            for (auto it = bb->instr.begin(); it != bb->instr.end(); it++) {
                Call* call = Call::Cast(*it);
                if (!call)
                    continue;
                MkClsFun* cls = MkClsFun::Cast(call->cls());
                if (!cls)
                    continue;
                Function* fun = cls->fun;
                if (!Query::noEnv(fun))
                    continue;
                if (fun->arg_name.size() != call->nargs() - 1)
                    continue;

                BB* split = BBTransform::split(++function->max_bb_id, bb, it);

                Call* newCall = Call::Cast(*split->instr.begin());
                std::vector<MkArg*> arguments;
                for (size_t i = 0; i < newCall->nargs() - 1; ++i) {
                    MkArg* a = MkArg::Cast(newCall->callArg(i));
                    assert(a);
                    arguments.push_back(a);
                }

                BB* copy = BBTransform::clone(&function->max_bb_id, fun->entry);
                bb->next0 = copy;

                // Find evaluation dominance order of LdArgs
                PromEvalAnalysis promeval(fun->arg_name.size(), copy);
                promeval();

                std::unordered_map<Instruction*, Value*> promiseResult;
                Visitor::run(copy, [&](BB* bb) {
                    for (auto it = bb->instr.begin(); it != bb->instr.end();
                         it++) {
                        Instruction* i = *it;
                        LdArg* ld = LdArg::Cast(*it);
                        if (!ld)
                            continue;
                        MkArg* a = arguments[ld->id];
                        Value* strict = a->arg<0>();
                        Instruction* dominatingLoad = promeval.loadedAt[i];
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
                            promiseResult[i] = promRes;
                            it = split->instr.begin();
                            ld = LdArg::Cast(*it);
                            assert(ld);
                            ld->replaceUsesWith(promRes);
                            it = split->remove(it);
                            bb = split;
                        }
                        if (it == bb->instr.end())
                            return;
                    }
                });

                Visitor::run(copy, [&](BB* bb) {
                    for (auto it = bb->instr.begin(); it != bb->instr.end();
                         it++) {
                        Instruction* i = *it;
                        LdArg* ld = LdArg::Cast(*it);
                        if (!ld)
                            continue;
                        MkArg* a = arguments[ld->id];
                        Instruction* dominatingLoad = promeval.loadedAt[i];
                        if (promiseResult.count(dominatingLoad))
                            ld->replaceUsesWith(
                                promiseResult.at(dominatingLoad));
                        else
                            ld->replaceUsesWith(a);
                        it = bb->remove(it);
                        if (it == bb->instr.end())
                            return;
                    }
                });

                Value* inlineeRes = BBTransform::forInline(copy, split);
                newCall->replaceUsesWith(inlineeRes);
                // Remove the call instruction
                split->remove(split->instr.begin());

                bb = split;
                it = split->instr.begin();
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
