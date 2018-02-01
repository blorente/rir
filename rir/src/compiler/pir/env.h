#ifndef COMPILER_ENV_H
#define COMPILER_ENV_H

#include "instruction_list.h"
#include "value.h"

#include <iostream>
#include <vector>

namespace rir {
namespace pir {

class Instruction;

class Env : public Value {
  public:
    static size_t envIdCount;

    size_t envId;
    Env* parent;

    Env(Env* parent = nullptr)
        : Value(RType::env, Tag::Value), envId(envIdCount++), parent(parent) {}
    void printRef(std::ostream& out) override;
};
}
}

#endif
