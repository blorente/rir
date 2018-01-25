#include "bb_clone.h"
#include "../pir/pir_impl.h"
#include "../util/visitor.h"

#include <unordered_map>

namespace rir {
namespace pir {

BB* BBClone::run(Function* fun) {
    std::vector<BB*> bbs(fun->max_bb_id + 1);

    // Copy instructions
    std::unordered_map<Value*, Value*> relocation_table;
    Visitor::run(fun->entry, [&](BB* bb) {
        BB* theClone = BB::cloneInstrs(bb);
        bbs[bb->id] = theClone;
        for (size_t i = 0; i < bb->instr.size(); ++i)
            relocation_table[bb->instr[i]] = theClone->instr[i];
    });

    // Fixup CFG
    Visitor::run(fun->entry, [&](BB* bb) {
        if (bb->next0)
            bbs[bb->id]->next0 = bbs[bb->next0->id];
        if (bb->next1)
            bbs[bb->id]->next1 = bbs[bb->next1->id];
    });

    // Relocate arg pointers
    BB* newEntry = bbs[fun->entry->id];
    Visitor::run(newEntry, [&](BB* bb) {
        for (auto i : bb->instr)
            i->map_arg([&](Value* v, PirType) {
                if (v->kind == Kind::instruction)
                    return relocation_table[v];
                else
                    return v;
            });
    });

    return newEntry;
}
}
}
