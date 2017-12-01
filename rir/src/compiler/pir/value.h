#ifndef COMPILER_VALUE_H
#define COMPILER_VALUE_H

#include "type.h"

#include <iostream>

namespace rir {
namespace pir {

enum class Kind : uint8_t { value, instruction };

class BB;

class Value {
  public:
    RType type;
    Kind kind;
    Value(RType type, Kind kind) : type(type), kind(kind) {}
    virtual void printRef(std::ostream& out) = 0;
    virtual BB* bb() = 0;
};
}
}

#endif
