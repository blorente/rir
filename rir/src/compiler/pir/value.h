#ifndef COMPILER_VALUE_H
#define COMPILER_VALUE_H

#include "type.h"

#include <functional>
#include <iostream>

namespace rir {
namespace pir {

enum class Tag : uint8_t;

class BB;

class Value {
  public:
    PirType type;
    Tag tag;
    Value(PirType type, Tag tag) : type(type), tag(tag) {}
    virtual void printRef(std::ostream& out) = 0;
    typedef std::function<void(BB*)> bbMaybe;
    virtual void bb(bbMaybe){};
    virtual ~Value(){};
};

}
}

#endif
