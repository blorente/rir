#ifndef COMPILER_PROMISE_H
#define COMPILER_PROMISE_H

#include "function.h"

namespace rir {
namespace pir {

class Promise : public Code {
  public:
    unsigned id;
    Promise(Env* e, unsigned id) : Code(e), id(id) {}
    void print(std::ostream& out = std::cout) {
        out << "Prom " << id << ":\n";
        Code::print(out);
    }

    friend std::ostream& operator<<(std::ostream& out, const Promise& p) {
        out << "Prom(" << p.id << ")";
        return out;
    }
};
}
}

#endif
