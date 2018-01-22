#ifndef COMPILER_VISITOR_H
#define COMPILER_VISITOR_H

#include "../pir/pir.h"

#include <functional>
#include <set>

namespace rir {
namespace pir {

class Visitor {
  public:
    typedef std::function<bool(BB*)> BBReturnAction;
    typedef std::function<void(BB*)> BBAction;
    static bool check(BB* bb, BBReturnAction action);
    static void run(BB* bb, BBAction action);
};
}
}

#endif
