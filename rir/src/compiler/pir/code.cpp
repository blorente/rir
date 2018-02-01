#include "code.h"
#include "../util/visitor.h"
#include "pir_impl.h"

#include <stack>

namespace rir {
namespace pir {

Code::Code(Env* env) : entry(new BB(0)), env(env) {}

void Code::print(std::ostream& out) {
    Visitor::run<true>(entry, [&out](BB* bb) { bb->print(out); });
}

Code::~Code() {
    std::stack<BB*> toDel;
    Visitor::run(entry, [&](BB* bb) { toDel.push(bb); });
    while (!toDel.empty()) {
        delete toDel.top();
        toDel.pop();
    }
}
}
}
