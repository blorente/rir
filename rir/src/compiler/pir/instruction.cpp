#include "instruction.h"
#include "pir_impl.h"

#include "../util/visitor.h"
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
    out << type << " ";
    if (type != PirType::voyd()) {
        printRef(out);
        out << " = ";
    }
    printRhs(out);
}

void Instruction::printRef(std::ostream& out) { out << "%" << id(); };

Instruction::Id Instruction::id() { return Id(bb()->id, bb()->indexOf(this)); }

bool Instruction::unused() {
    // TODO: better solution?
    if (tag == ITag::Branch || tag == ITag::Return)
        return false;

    return Visitor::check(bb(), [&](BB* bb) {
        bool unused = true;
        for (auto i : bb->instr) {
            i->each_arg(
                [&](Value* v, PirType t) { unused = unused && (v != this); });
            if (!unused)
                return false;
        }
        return unused;
    });
}

void Instruction::replaceUsesWith(Value* replace) {
    Visitor::run(bb(), [&](BB* bb) {
        for (auto i : bb->instr) {
            i->map_arg([&](Value* v, PirType t) {
                if (v == this)
                    return replace;
                else
                    return v;
            });
        }
    });
}

void LdConst::printRhs(std::ostream& out) {
    std::string val;
    {
        CaptureOut rec;
        Rf_PrintValue(c);
        val = rec();
    }
    if (val.length() > 0)
        val.pop_back();
    out << name() << " " << val;
}

void Branch::printRhs(std::ostream& out) {
    out << name() << " ";
    arg<0>()->printRef(out);
    out << ", BB" << bb()->next0->id << ", BB" << bb()->next1->id;
}

void MkArg::printRhs(std::ostream& out) {
    out << name() << "(";
    arg<0>()->printRef(out);
    out << ", " << *prom << ") " << *env();
}

void MkClsFun::printRhs(std::ostream& out) {
    out << name() << "(" << *fun << ") " << *env();
}

void LdVar::printRhs(std::ostream& out) {
    out << name() << "(" << CHAR(PRINTNAME(varName)) << ")";
    out << " " << *env();
}

void LdFun::printRhs(std::ostream& out) {
    out << name() << "(" << CHAR(PRINTNAME(varName)) << ")";
    out << " " << *env();
}

void LdArg::printRhs(std::ostream& out) {
    out << name() << "(" << id << ")";
}

void StVar::printRhs(std::ostream& out) {
    out << name() << "(" << CHAR(PRINTNAME(varName)) << ", ";
    val()->printRef(out);
    out << ") " << *env();
}

void Call::printRhs(std::ostream& out) {
    out << name() << " ";
    this->arg(0)->printRef(out);
    out << " (";
    for (unsigned i = 1; i < nargs() - 1; ++i) {
        this->arg(i)->printRef(out);
        out << ", ";
    }
    this->arg(nargs() - 1)->printRef(out);
    out << ") " << *env();
}

void Phi::updateType() {
    type = arg(0)->type;
    each_arg([&](Value* v, PirType t) -> void { type = type | v->type; });
}
}
}
