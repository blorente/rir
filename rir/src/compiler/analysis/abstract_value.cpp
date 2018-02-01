#include "../pir/pir_impl.h"
#include "query.h"
#include "scope.h"

namespace rir {
namespace pir {

AbstractValue::AbstractValue(Value* v) : type(v->type) { vals.insert(v); }

bool AbstractValue::merge(const AbstractValue& other) {
    assert(other.type != PirType::bottom());

    if (unknown)
        return false;
    if (type == PirType::bottom()) {
        *this = other;
        return true;
    }
    if (other.unknown) {
        taint();
        return true;
    }

    bool changed = false;
    if (!std::includes(vals.begin(), vals.end(), other.vals.begin(),
                       other.vals.end())) {
        vals.insert(other.vals.begin(), other.vals.end());
        changed = true;
    }
    if (!std::includes(args.begin(), args.end(), other.args.begin(),
                       other.args.end())) {
        args.insert(other.args.begin(), other.args.end());
        changed = true;
    }

    return changed;
}

void AbstractValue::print(std::ostream& out) {
    if (unknown) {
        out << "??";
        return;
    }
    out << "(";
    for (auto it = vals.begin(); it != vals.end();) {
        (*it)->printRef(out);
        it++;
        if (it != vals.end() && args.size() != 0)
            out << "|";
    }
    for (auto it = args.begin(); it != args.end();) {
        out << *it;
        it++;
        if (it != args.end())
            out << "|";
    }
    out << ") : " << type;
}
}
}
