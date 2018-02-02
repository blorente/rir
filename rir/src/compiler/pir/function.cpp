#include "function.h"
#include "pir_impl.h"

#include <iostream>

namespace rir {
namespace pir {

void Function::print(std::ostream& out) {
    out << "Function " << this << " ";
    env->printRef(out);
    out << "\n";
    Code::print(out);
    for (auto p : promise) {
        if (p)
            p->print(out);
    }
}

Promise* Function::createProm() {
    Promise* p = new Promise(env, this, promise.size());
    promise.push_back(p);
    return p;
}

Function::~Function() {
    for (auto p : promise)
        if (p)
            delete p;
    for (auto p : default_arg)
        delete p;
}
}
}
