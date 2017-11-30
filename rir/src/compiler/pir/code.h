#ifndef COMPILER_CODE_H
#define COMPILER_CODE_H

#include "bb.h"
#include "env.h"

namespace rir {
namespace pir {

class Code {
  public:
    BB* entry;
    Env* env;

    Code(Env* env) : entry(new BB(0)), env(env) {}
    void print(std::ostream& = std::cout);
    ~Code();
};
}
}

#endif
