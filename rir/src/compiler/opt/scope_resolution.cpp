#include "scope_resolution.h"
#include "../pir/pir_impl.h"

namespace {

using namespace rir::pir;

class TheScopeResolution {
  public:
    Function* function;
    TheScopeResolution(Function* function) : function(function) {}
    void operator()();
};
}

namespace rir {
namespace pir {

void ScopeResolution::apply(Function* function) {}
}
}
