#include "verifier.h"
#include "visitor.h"

namespace rir {
namespace pir {

void Verifier::operator()() {
    Visitor<Verifier> visit(this);
    visit(f->entry);

    for (auto p : f->promise)
        verify(p);
    for (auto p : f->default_arg)
        verify(p);
}

void Verifier::verify(BB* bb) {
    for (auto i : bb->instr)
        verify(i, bb);
    Instruction* last = bb->last();
    if ((Branch::cast(last)))
        assert(bb->next0 && bb->next1);
    else if ((Return::cast(last)))
        assert(!bb->next0 && !bb->next1);
    else
        assert(bb->next0 && !bb->next1);
}

void Verifier::verify(Promise* p) {
    Visitor<Verifier> visit(this);
    visit(p->entry);
}

void Verifier::verify(Instruction* i, BB* bb) {
    i->each_arg([i, bb](Value* v, RType t) -> void {
        if (!subtype(v->type, t)) {
            std::cerr << "Error at instruction '";
            i->print(std::cerr);
            std::cerr << "': Value ";
            v->printRef(std::cerr);
            std::cerr << " has type " << v->type
                      << " which is not a subtype of " << t << "\n";
            assert(false);
        }
        if (v->bb() != bb) {
            std::cerr << "Error at instruction '";
            i->print(std::cerr);
            std::cerr << "': Value ";
            v->printRef(std::cerr);
            std::cerr << " does not come from the same BB\n";
            assert(false);
        }
    });
}
}
}
