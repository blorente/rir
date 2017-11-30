#include "function.h"
#include "promise.h"

#include <iostream>

namespace rir {
namespace pir {

void Function::print(std::ostream& out) {
    out << "Function \n";
    Code::print(out);
    for (auto p : promise) {
        p->print(out);
    }
}

Promise* Function::createProm() {
    Promise* p = new Promise(env, promise.size());
    promise.push_back(p);
    return p;
}
}
}
