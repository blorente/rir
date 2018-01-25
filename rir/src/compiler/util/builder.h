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
    Builder(Function* fun, Code* code, Env* env, BB* bb)
        : function(fun), code(code), env(env), bb(bb) {}

    template <class T>
    T* operator()(T* i) {
        bb->append(i);
        i->bb_ = bb;
        return i;
    }

    BB* createBB();

    void next(BB* b) {
        bb->next0 = b;
        bb = b;
    }
};
}
}

#endif
