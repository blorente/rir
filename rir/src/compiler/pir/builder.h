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

    template <class T>
    T* operator()(T* i) {
        *bb << i;
        return i;
    }

    BB* createBB() { return new BB(id++); }

    void next(BB* b) {
        bb->next0 = b;
        bb = b;
    }
};
}
}

#endif
