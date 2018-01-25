#ifndef BB_CLONE_H
#define BB_CLONE_H

#include "../pir/pir.h"

namespace rir {
namespace pir {

class BBClone {
  public:
    static BB* run(Function* src);
};
}
}

#endif
