#include "builder.h"
#include "../pir/pir_impl.h"

namespace rir {
namespace pir {

BB* Builder::createBB() { return new BB(++function->max_bb_id); }
}
}
