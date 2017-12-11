#include "bb.h"
#include "env.h"
#include "instruction.h"

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
}
}
