#include "bb.h"
#include "builder.h"
#include "env.h"
#include "instruction.h"
#include "verifier.h"

using namespace rir;
using namespace pir;

extern void compiler_tests() {
    Env* e = new Env();
    Function* f = new Function({}, e);

    Builder b(f);
    auto c = b(new LdConst(R_NilValue));
    b(new StVar("x", c, e));
    auto l = b(new LdVar("x", e));
    auto k = b(new ChkMissing(l));

    auto fun = b(new LdFun("c", e));

    Promise* arg1p = f->createProm();
    {
        Builder pb(arg1p);
        auto r = pb(new LdVar("x", e));
        auto k = pb(new ChkMissing(r));
        pb(new Return(k));
    }

    auto arg1 = b(new MkArg(arg1p, e));
    auto call = b(new Call<2>(e, fun, {{arg1, k}}));
    auto mval = b(new Force(call));
    auto val = b(new ChkMissing(mval));

    b(new Return(val));

    Verifier v(f);
    v();

    f->print(std::cout);

    delete f;
    delete e;
}
