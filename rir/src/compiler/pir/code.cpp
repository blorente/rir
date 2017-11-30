#include "code.h"
#include "visitor.h"

namespace {
using namespace rir::pir;

class Deleter {
  public:
    Visitor<Deleter> v;
    Deleter() : v(this) {}
    void accept(BB* bb) { delete bb; }
    void operator()(BB* bb) { v(bb); }
};

class Printer {
  public:
    Visitor<Printer> v;
    std::ostream& out;
    Printer(std::ostream& out) : v(this), out(out) {}
    void accept(BB* bb) { bb->print(out); }
    void operator()(BB* bb) { v(bb); }
};
}

namespace rir {
namespace pir {

void Code::print(std::ostream& out) {
    Printer p(out);
    p(entry);
}

Code::~Code() {
    Deleter d;
    d(entry);
}
}
}
