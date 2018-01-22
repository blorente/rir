#ifndef COMPILER_FUNCTION_H
#define COMPILER_FUNCTION_H

#include "R/r.h"
#include "code.h"
#include "pir.h"

namespace rir {
namespace pir {

class Function : public Code {
  public:
    Function(std::initializer_list<SEXP> a, std::initializer_list<Promise*> p,
             Env* e)
        : Code(e), arg_name(a), default_arg(p) {}

    Function(const std::vector<SEXP>& a, const std::vector<Promise*>& p, Env* e)
        : Code(e), arg_name(a), default_arg(p) {}

    Function(std::initializer_list<SEXP> a, Env* e) : Function(a, {}, e) {}
    Function(const std::vector<SEXP>& a, Env* e) : Function(a, {}, e) {}

    std::vector<SEXP> arg_name;
    std::vector<Promise*> default_arg;

    std::vector<Promise*> promise;

    void print(std::ostream& out = std::cout);

    Promise* createProm();

    friend std::ostream& operator<<(std::ostream& out, const Function& e) {
        out << "Func(" << (void*)&e << ")";
        return out;
    }

    ~Function();
};
}
}

#endif
