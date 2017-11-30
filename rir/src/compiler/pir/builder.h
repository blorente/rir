#ifndef COMPILER_BUILDER_H
#define COMPILER_BUILDER_H

#include "function.h"

namespace rir {
namespace pir {

class Builder {
  public:
    Code* f;
    Env* e;
    BB* bb;
    unsigned id;
    Builder(Code* f) : f(f), e(f->env), bb(f->entry), id(bb->id + 1) {}

    Instruction* operator()(Instruction* i) {
        *bb << i;
        return i;
    }

    void createNext() { bb = bb->createNext(id++); }
};
}
}

#endif
