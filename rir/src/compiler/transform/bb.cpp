#include "bb.h"
#include "../pir/pir_impl.h"
#include "../util/visitor.h"

#include <unordered_map>

namespace rir {
namespace pir {

BB* BBTransform::clone(size_t* id_counter, BB* src) {
    std::vector<BB*> bbs;

    // Copy instructions
    std::unordered_map<Value*, Value*> relocation_table;
    Visitor::run(src, [&](BB* bb) {
        BB* theClone = BB::cloneInstrs(bb);
        if (bb->id >= bbs.size())
            bbs.resize(bb->id + 5);
        bbs[bb->id] = theClone;
        for (size_t i = 0; i < bb->instr.size(); ++i)
            relocation_table[bb->instr[i]] = theClone->instr[i];
    });

    // Fixup CFG
    Visitor::run(src, [&](BB* bb) {
        if (bb->next0)
            bbs[bb->id]->next0 = bbs[bb->next0->id];
        if (bb->next1)
            bbs[bb->id]->next1 = bbs[bb->next1->id];
    });

    // Relocate arg pointers
    BB* newEntry = bbs[src->id];
    Visitor::run(newEntry, [&](BB* bb) {
        for (auto i : bb->instr)
            i->map_arg([&](Value* v, PirType) {
                if (v->kind == Kind::instruction)
                    return relocation_table[v];
                else
                    return v;
            });
        *id_counter = *id_counter + 1;
        bb->id = *id_counter;
    });

    return newEntry;
}

BB* BBTransform::split(size_t next_id, BB* src, BB::Instrs::iterator it) {
    BB* split = new BB(next_id);
    split->next0 = src->next0;
    split->next1 = src->next1;
    while (it != src->instr.end()) {
        it = src->moveTo(it, split);
    }
    src->next0 = split;
    src->next1 = nullptr;
    return split;
}

Value* BBTransform::forInline(BB* inlinee, BB* splice) {
    Value* found = nullptr;
    Visitor::run(inlinee, [&](BB* bb) {
        if (bb->next0 != nullptr)
            return;

        assert(bb->next1 == nullptr);
        Return* ret = Return::Cast(bb->instr.back());
        assert(ret);
        assert(!found);
        found = ret->arg<0>();
        bb->next0 = splice;
        bb->remove(bb->instr.end() - 1);
    });
    assert(found);
    return found;
}
}
}
