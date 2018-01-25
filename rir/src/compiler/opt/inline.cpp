#include "inline.h"
#include "../analysis/query.h"
#include "../pir/pir_impl.h"
#include "../util/bb_clone.h"
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

                BB* split = new BB(++function->max_bb_id);
                split->next0 = bb->next0;
                split->next1 = bb->next1;

                it = bb->moveTo(it, split);
                it++;
                Call* newCall = Call::Cast(*split->instr.begin());
                for (; it != bb->instr.end(); ++it) {
                    it = bb->moveTo(it, split);
                }
                std::vector<MkArg*> arguments;
                for (size_t i = 0; i < newCall->nargs() - 1; ++i) {
                    MkArg* a = MkArg::Cast(newCall->callArg(i));
                    assert(a);
                    arguments.push_back(a);
                }

                BB* copy = BBClone::run(fun);
                Visitor::run(
                    copy, [&](BB* bb) { bb->id += function->max_bb_id + 1; });
                function->max_bb_id += fun->max_bb_id + 1;

                bb->next0 = copy;
                bb->next1 = nullptr;

                bool found_ret = false;
                Visitor::run(copy, [&](BB* bb) {
                    for (auto it = bb->instr.begin(); it != bb->instr.end();
                         it++) {
                        LdArg* ld = LdArg::Cast(*it);
                        if (!ld)
                            continue;
                        MkArg* a = arguments[ld->id];
                        Value* strict = a->arg<0>();
                        // Todo: actually lazy args need another BB splicing foo
                        // here!!
                        assert(strict != Missing::instance());
                        ld->replaceUsesWith(strict);
                        it = bb->remove(it);
                    }

                    if (bb->next0 == nullptr) {
                        assert(bb->next1 == nullptr);
                        assert(!found_ret);
                        bb->next0 = split;
                        Return* ret = Return::Cast(bb->instr.back());
                        if (!ret)
                            asm("int3");
                        newCall->replaceUsesWith(ret->arg<0>());
                        bb->remove(bb->instr.end() - 1);
                        found_ret = true;
                    }
                });
                assert(found_ret);

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
