#include "module.h"
#include "function.h"

namespace rir {
namespace pir {

void Module::print(std::ostream& out) {
    for (auto f : function)
        f->print(out);
}

Module::~Module() {
    for (auto f : function)
        delete f;
    for (auto e : env)
        delete e;
}
}
}
