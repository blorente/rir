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
                    std::set<Value*> phin;
                    phi->each_arg([&](Value* v, PirType) { phin.insert(v); });
                    if (phin.size() < phi->nargs()) {
                        Phi* newphi = new Phi;
                        for (auto v : phin)
                            newphi->push_arg(v);
                        phi->replaceUsesWith(newphi);
                        bb->replace(it, newphi);
                        phi = newphi;
                    }
                    phi->updateType();
                    if (phin.size() == 1) {
                        phi->replaceUsesWith(*phin.begin());
                        it = bb->remove(it);
                    }
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

        CFG cfg(function->entry);
        std::unordered_map<BB*, BB*> toDel;

        Visitor::run(function->entry, [&](BB* bb) {
            // Remove unnecessary splits
            if (bb->jmp() && cfg.preds(bb->next0).size() == 1) {
                BB* d = bb->next0;
                while (d->instr.size() > 0) {
                    d->moveTo(d->instr.begin(), bb);
                }
                bb->next0 = d->next0;
                bb->next1 = d->next1;
                d->next0 = nullptr;
                d->next1 = nullptr;
                toDel[d] = nullptr;
            } else
                // Remove empty jump-through blocks
                if (bb->jmp() && bb->next0->instr.empty() && bb->next0->jmp() &&
                    cfg.preds(bb->next0->next0).size() == 1) {
                toDel[bb->next0] = bb->next0->next0;
            } else
                // Remvove empty branches
                if (bb->next0 && bb->next1) {
                assert(bb->next0->jmp() && bb->next1->jmp());
                if (bb->next0->empty() && bb->next1->empty() &&
                    bb->next0->next0 == bb->next1->next0) {
                    toDel[bb->next0] = bb->next0->next0;
                    toDel[bb->next1] = bb->next0->next0;
                    bb->next1 = nullptr;
                    bb->remove(bb->instr.end() - 1);
                }
            }
        });
        if (function->entry->jmp() && function->entry->empty()) {
            toDel[function->entry] = function->entry->next0;
            function->entry = function->entry->next0;
        }
        if (function->entry->jmp() &&
            cfg.preds(function->entry->next0).size() == 1) {
            BB* bb = function->entry;
            BB* d = bb->next0;
            while (d->instr.size() > 0) {
                d->moveTo(d->instr.begin(), bb);
            }
            bb->next0 = d->next0;
            bb->next1 = d->next1;
            d->next0 = nullptr;
            d->next1 = nullptr;
            toDel[d] = nullptr;
        }
        Visitor::run(function->entry, [&](BB* bb) {
            while (toDel.count(bb->next0))
                bb->next0 = toDel[bb->next0];
            assert(!toDel.count(bb->next1));
        });
        for (auto e : toDel) {
            BB* bb = e.first;
            bb->next0 = nullptr;
            delete bb;
        }

        // Renumber
        function->max_bb_id = 0;
        Visitor::run(function->entry,
                     [&](BB* bb) { bb->id = function->max_bb_id++; });
        function->max_bb_id--;
    }
};
}

namespace rir {
namespace pir {

void Cleanup::apply(Function* function) {
    TheCleanup s(function);
    s();
    s();
}
}
}
