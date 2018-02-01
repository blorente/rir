#ifndef PIR_ABSTRACT_VALUE_H
#define PIR_ABSTRACT_VALUE_H

#include "../pir/pir.h"

#include <algorithm>
#include <set>
#include <unordered_map>

namespace rir {
namespace pir {

struct AbstractValue {
    bool unknown = false;

    std::set<Value*> vals;
    std::set<size_t> args;

    PirType type = PirType::bottom();

    AbstractValue(PirType t = PirType::bottom()) : type(t) {}
    AbstractValue(Value* v);

    static AbstractValue arg(size_t id) {
        AbstractValue v(PirType::any());
        v.args.insert(id);
        return v;
    }

    static AbstractValue tainted() {
        AbstractValue v(PirType::any());
        v.taint();
        return v;
    }

    void taint() {
        vals.clear();
        args.clear();
        unknown = true;
        type = PirType::any();
    }

    bool isUnknown() const { return unknown; }

    bool singleValue() {
        if (unknown)
            return false;
        return vals.size() == 1 && args.size() == 0;
    }
    bool singleArg() {
        if (unknown)
            return false;
        return args.size() == 1 && vals.size() == 0;
    }

    bool merge(const AbstractValue& other);

    void print(std::ostream& out = std::cout);
};
}
}

#endif
