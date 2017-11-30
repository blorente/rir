#ifndef COMPILER_VALUE_H
#define COMPILER_VALUE_H

#include "type.h"

#include <iostream>

namespace rir {
namespace pir {

class BB;

class Value {
  public:
    RType type;
    Value(RType type) : type(type) {}
    virtual void printRef(std::ostream& out) = 0;
    virtual BB* bb() = 0;
};
}
}

#endif
