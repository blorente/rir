#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

#include <cassert>
#include <cstdint>
#include <iostream>

#include "utils/Bitset.h"

namespace rir {
namespace pir {

enum class RBaseType : uint8_t {
    unused,

    //                                          maybeVal (val?)   arg   any

    symbol, // Value types              X                 X     X
    logical,
    closure,

    missing, // R_MissingArg             X                       X

    prom, // Unevaluated promise                        X     X

    voyd, // void

    test, // machine boolean

    /* unused */ max
};

typedef BitSet<uint8_t, RBaseType> RType;

struct RTypes {
    static RType val() {
        static RType v = RType(RBaseType::symbol) | RType(RBaseType::logical) |
                         RType(RBaseType::closure);
        return v;
    }
    static RType arg() {
        static RType v = RType(RBaseType::prom) | val();
        return v;
    }
    static RType maybeVal() {
        static RType v = RType(RBaseType::missing) | val();
        return v;
    }
    static RType any() {
        static RType v = RType(RBaseType::prom) | maybeVal();
        return v;
    }
};

inline bool subtype(RType a, RType b) { return b.includes(a); }

inline std::ostream& operator<<(std::ostream& out, RBaseType t) {
    switch (t) {
    case RBaseType::voyd:
        out << "void";
        break;
    case RBaseType::closure:
        out << "cls";
        break;
    case RBaseType::prom:
        out << "prm";
        break;
    case RBaseType::test:
        out << "t";
        break;
    case RBaseType::symbol:
        out << "sym";
        break;
    case RBaseType::logical:
        out << "lgl";
        break;
    case RBaseType::missing:
        out << "miss";
        break;
    case RBaseType::unused:
    case RBaseType::max:
        assert(false);
        break;
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, RType t) {
    if (t == RTypes::val()) {
        out << "val";
    } else if (t == RTypes::arg()) {
        out << "arg";
    } else if (t == RTypes::maybeVal()) {
        out << "val?";
    } else if (t == RTypes::any()) {
        out << "any";
    } else {
        size_t found = 0;
        for (RBaseType bt = (RBaseType)0; bt != RBaseType::max;
             bt = (RBaseType)((size_t)bt + 1)) {
            if (t.includes(bt)) {
                ++found;
                out << bt;
                if (found < t.size())
                    out << ", ";
            }
        }
    }
    return out;
}
}
}

#endif
