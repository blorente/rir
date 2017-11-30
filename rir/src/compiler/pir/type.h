#ifndef COMPILER_TYPE_H
#define COMPILER_TYPE_H

#include <cassert>
#include <cstdint>
#include <iostream>

namespace rir {
namespace pir {

enum class RType : uint8_t {
    unused,
    value,
    prom,
    arg,
    symbol,
    voyd,
    test,
    any,
    maybeVal,
    closure,
};

inline bool subtype(RType a, RType b) {
    if (a == b)
        return true;
    if (b == RType::any)
        return true;

    switch (a) {
    case RType::voyd:
    case RType::closure:
    case RType::test:
    case RType::arg:
    case RType::any:
    case RType::maybeVal:
        return false;
    case RType::prom:
        return b == RType::arg;
    case RType::value:
        return b == RType::arg || b == RType::value;
    case RType::symbol:
        return b == RType::symbol || b == RType::value;
    case RType::unused:
        assert(false);
    }
    assert(false);
    return false;
}

inline std::ostream& operator<<(std::ostream& out, RType t) {
    switch (t) {
    case RType::voyd:
        out << "void";
        break;
    case RType::closure:
        out << "cls";
        break;
    case RType::any:
        out << "any";
        break;
    case RType::value:
        out << "val";
        break;
    case RType::arg:
        out << "arg";
        break;
    case RType::prom:
        out << "prm";
        break;
    case RType::maybeVal:
        out << "val?";
        break;
    case RType::test:
        out << "t";
        break;
    case RType::symbol:
        out << "sym";
        break;
    case RType::unused:
        assert(false);
        break;
    }
    return out;
}
}
}

#endif
