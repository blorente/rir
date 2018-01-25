#include "cleanup.h"
#include "../pir/pir_impl.h"
#include "../util/cfg.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <unordered_map>

namespace {
using namespace rir::pir;

class TheCleanup {
  public:
    TheCleanup(Function* function) : function(function) {}
    Function* function;
    void operator()() {
        std::set<size_t> used_p;

        Visitor::run(function->entry, [&](BB* bb) {
            for (auto it = bb->instr.begin(); it != bb->instr.end(); it++) {
                Instruction* i = *it;
                Force* force = Force::Cast(i);
                ChkMissing* missing = ChkMissing::Cast(i);
                ChkClosure* closure = ChkClosure::Cast(i);
                Phi* phi = Phi::Cast(i);
                MkArg* arg = MkArg::Cast(i);
                if (!i->mightIO() && !i->changesEnv() && i->unused()) {
                    it = bb->remove(it);
                } else if (force) {
                    Value* arg = force->arg<0>();
                    if (PirType::valOrMissing() >= arg->type) {
                        force->replaceUsesWith(arg);
                        it = bb->remove(it);
                    }
                } else if (missing) {
                    Value* arg = missing->arg<0>();
                    if (PirType::val() >= arg->type) {
                        missing->replaceUsesWith(arg);
                        it = bb->remove(it);
                    }
                } else if (closure) {
                    Value* arg = closure->arg<0>();
                    if (PirType::val() >= arg->type) {
                        closure->replaceUsesWith(arg);
                        it = bb->remove(it);
                    }
                } else if (phi) {
                    phi->updateType();
                } else if (arg) {
                    used_p.insert(arg->prom->id);
                }
                if (it == bb->instr.end())
                    break;
            }
        });

        for (size_t i = 0; i < function->promise.size(); ++i) {
            if (used_p.find(i) == used_p.end()) {
                delete function->promise[i];
                function->promise[i] = nullptr;
            }
        }
    }
};
}

namespace rir {
namespace pir {

void Cleanup::apply(Function* function) {
    TheCleanup s(function);
    s();
}
}
}
