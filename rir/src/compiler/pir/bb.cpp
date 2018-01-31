#include "pir_impl.h"

#include <iostream>

namespace rir {
namespace pir {

BB::BB(unsigned id) : id(id) {}

void BB::print(std::ostream& out) {
    out << "BB " << id << "\n";
    for (auto i : instr) {
        out << "  ";
        i->print(out);
        out << "\n";
    }
    if (next0 && !next1)
        out << "  goto BB " << next0->id << "\n";
}

BB::~BB() {
    for (auto* i : instr)
        delete i;
}

void BB::append(Instruction* i) {
    instr.push_back(i);
    i->bb_ = this;
}

BB::Instrs::iterator BB::remove(Instrs::iterator it) {
    delete *it;
    return instr.erase(it);
}

BB::Instrs::iterator BB::moveTo(Instrs::iterator it, BB* other) {
    Instruction* i = *it;
    i->bb_ = other;
    other->instr.push_back(i);
    return instr.erase(it);
}

BB* BB::cloneInstrs(BB* src) {
    BB* c = new BB(src->id);
    for (auto i : src->instr) {
        Instruction* ic = i->clone();
        ic->bb_ = c;
        c->instr.push_back(ic);
    }
    c->next0 = c->next1 = nullptr;
    return c;
}

void BB::replace(Instrs::iterator it, Instruction* i) {
    delete *it;
    *it = i;
    i->bb_ = this;
}

BB::Instrs::iterator BB::insert(Instrs::iterator it, Instruction* i) {
    auto itup = instr.insert(it, i);
    i->bb_ = this;
    return itup;
}
}
}
