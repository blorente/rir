#include "inline.h"
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

                Visitor::run(copy, [&](BB* bb) {
                    for (auto it = bb->instr.begin(); it != bb->instr.end();
                         it++) {
                        LdArg* ld = LdArg::Cast(*it);
                        if (!ld)
                            continue;
                        MkArg* a = arguments[ld->id];
                        Value* strict = a->arg<0>();
                        if (strict != Missing::instance()) {
                            ld->replaceUsesWith(strict);
                            it = bb->remove(it);
                        } else {
                            Promise* prom = a->prom;
                            BB* split = BBTransform::split(
                                ++function->max_bb_id, bb, it);
                            BB* prom_copy = BBTransform::clone(
                                &function->max_bb_id, prom->entry);
                            bb->next0 = prom_copy;
                            Value* promRes =
                                BBTransform::forInline(prom_copy, split);
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
