#ifndef COMPILER_ENV_H
#define COMPILER_ENV_H

#include <iostream>
#include <vector>

namespace rir {
namespace pir {

class Instruction;

class Env {
  public:
    Env* parent;

    Env(Env* parent = nullptr) : parent(parent) {}

    std::vector<Instruction*> escape;
    friend std::ostream& operator<<(std::ostream& out, const Env& e) {
        out << "Env(" << (void*)&e << ")";
        return out;
    }
};
}
}

#endif
