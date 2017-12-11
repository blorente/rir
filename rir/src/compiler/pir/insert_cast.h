#ifndef COMPILER_PIR_INSERT_CAST_H
#define COMPILER_PIR_INSERT_CAST_H

#include "function.h"
#include "promise.h"

namespace rir {
namespace pir {

class InsertCast {
    BB* start;

  public:
    InsertCast(BB* s) : start(s) {}
    void operator()();
    void accept(BB* b);
};
}
}

#endif
