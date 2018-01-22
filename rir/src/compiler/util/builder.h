#ifndef COMPILER_BUILDER_H
#define COMPILER_BUILDER_H

#include "../pir/bb.h"
#include "../pir/pir.h"

namespace rir {
namespace pir {

class Builder {
  public:
    Function* function;
    Code* code;
    Env* env;
    BB* bb;
    unsigned id;
    Builder(Function* fun, Code* code, Env* env, BB* bb)
        : function(fun), code(code), env(env), bb(bb), id(bb->id + 1) {}

    template <class T>
    T* operator()(T* i) {
        bb->append(i);
        i->bb_ = bb;
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
