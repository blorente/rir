#ifndef COMPILER_PIR_VERIFIER_H
#define COMPILER_PIR_VERIFIER_H

#include "function.h"
#include "promise.h"

namespace rir {
namespace pir {

class Verifier {
    Function* f;

  public:
    Verifier(Function* f) : f(f) {}
    void operator()();
    void verify(BB*);
    void verify(Instruction*, BB* bb);
    void verify(Promise*);
    void accept(BB* b) { verify(b); }
};
}
}

#endif
