#ifndef COMPILER_VALUE_H
#define COMPILER_VALUE_H

#include "type.h"

#include <functional>
#include <iostream>

namespace rir {
namespace pir {

enum class Kind : uint8_t { value, instruction };

class BB;

class Value {
  public:
    PirType type;
    Kind kind;
    Value(PirType type, Kind kind) : type(type), kind(kind) {}
    virtual void printRef(std::ostream& out) = 0;
    typedef std::function<void(BB*)> bbMaybe;
    virtual void bb(bbMaybe){};
};

template <typename T>
class SingletonValue : public Value {
  public:
    SingletonValue(PirType t) : Value(t, Kind::value) {}
    static T* instance() {
        static T i;
        return &i;
    }
};

class Nil : public SingletonValue<Nil> {
  public:
    void printRef(std::ostream& out) override { out << "nil"; }

  private:
    friend class SingletonValue;
    Nil() : SingletonValue(RType::nil) {}
};

class Missing : public SingletonValue<Missing> {
  public:
    void printRef(std::ostream& out) override { out << "missing"; }

  private:
    friend class SingletonValue;
    Missing() : SingletonValue(PirType::missing()) {}
};
}
}

#endif
