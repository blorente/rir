#ifndef COMPILER_CODE_H
#define COMPILER_CODE_H

#include "pir.h"

namespace rir {
namespace pir {

class Code {
  public:
    BB* entry;
    Env* env;

    Code(Env* env);
    void print(std::ostream& = std::cout);
    ~Code();
};
}
}

#endif
