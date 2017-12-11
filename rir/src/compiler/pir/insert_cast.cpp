#include "insert_cast.h"
#include "visitor.h"

namespace rir {
namespace pir {

static pir::Instruction* cast(pir::Value* v, RType t) {
    if (v->type.includes(RBaseType::prom) && !t.includes(RBaseType::prom)) {
        return new pir::Force(v);
    }
    if (v->type.includes(RBaseType::missing) &&
        !t.includes(RBaseType::missing)) {
        return new pir::ChkMissing(v);
    }
    if (v->type == RBaseType::logical && t == RBaseType::test) {
        return new pir::AsTest(v);
    }

    std::cerr << "Cannot cast " << v->type << " to " << t << "\n";
    assert(false);
    return nullptr;
}

void InsertCast::operator()() {
    Visitor<InsertCast> visit(this);
    visit(start);
}

void InsertCast::accept(BB* bb) {
    for (auto i = bb->instr.begin(); i != bb->instr.end(); ++i) {
        auto instr = *i;
        Phi* p = nullptr;
        if ((p = Phi::cast(instr))) {
            RType out;
            instr->each_arg(
                [&out](Value* v, RType t) -> void { out = out | t; });
            p->type = out;
        }
        instr->map_arg([&i, bb](Value* v, RType t) -> Value* {
            size_t added = 0;
            while (!subtype(v->type, t)) {
                auto c = cast(v, t);
                c->bb_ = bb;
                v = c;
                i = bb->instr.insert((i + added), c);
                added++;
            }
            return v;
        });
    }
}
}
}
