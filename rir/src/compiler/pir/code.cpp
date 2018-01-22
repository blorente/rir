#include "code.h"
#include "../util/visitor.h"
#include "pir_impl.h"

namespace rir {
namespace pir {

Code::Code(Env* env) : entry(new BB(0)), env(env) {}

void Code::print(std::ostream& out) {
    Visitor::run(entry, [&out](BB* bb) { bb->print(out); });
}

Code::~Code() {
    Visitor::run(entry, [](BB* bb) { delete bb; });
}
}
}
