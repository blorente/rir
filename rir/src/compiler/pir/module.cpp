#include "module.h"
#include "pir_impl.h"

namespace rir {
namespace pir {

void Module::print(std::ostream& out) {
    for (auto f : function) {
        f->print(out);
        out << "\n";
    }
}

Module::~Module() {
    for (auto f : function)
        delete f;
    for (auto e : env)
        delete e;
}
}
}
