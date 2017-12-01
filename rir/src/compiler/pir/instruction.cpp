#include "instruction.h"
#include "bb.h"
#include "promise.h"

#include "utils/capture_out.h"

#include <cassert>

extern "C" SEXP deparse1line(SEXP call, Rboolean abbrev);

namespace rir {
namespace pir {

extern std::ostream& operator<<(std::ostream& out, Instruction::Id id) {
    out << std::get<0>(id) << "." << std::get<1>(id);
    return out;
}

void Instruction::print(std::ostream& out) {
    out << type;
    out << " ";
    printRef(out);
    out << " = ";
    printRhs(out);
}

void Instruction::printRef(std::ostream& out) { out << "%" << id(); };

Instruction::Id Instruction::id() { return Id(bb()->id, bb()->indexOf(this)); }

void LdConst::printRhs(std::ostream& out) {
    std::string val;
    {
        CaptureOut rec;
        Rf_PrintValue(c);
        val = rec();
    }
    if (val.length() > 0)
        val.pop_back();
    out << "ldconst " << val;
}

void Branch::printRhs(std::ostream& out) {
    out << "branch ";
    arg<0>()->printRef(out);
    out << ", BB" << bb()->next0->id << ", BB" << bb()->next1->id;
}

void MkArg::printRhs(std::ostream& out) {
    out << "mkarg " << *prom << " " << *env;
}
}
}
