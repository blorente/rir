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
        out << "<goto> BB " << next0->id << "\n";
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
