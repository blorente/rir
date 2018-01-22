#include "analysis/verifier.h"
#include "pir/pir_impl.h"
#include "util/builder.h"

using namespace rir;
using namespace pir;

extern void compiler_tests() {
    Env* e = new Env();
    Function* f = new Function({}, e);

    Builder b(f, f, e, f->entry);
    b(new StVar("x", Nil::instance(), e));
    auto l = b(new LdVar("x", e));
    auto lf = b(new Force(l));
    auto k = b(new ChkMissing(lf));

    auto fun = b(new LdFun("c", e));

    Promise* arg1p = f->createProm();
    {
        Builder pb(f, arg1p, e, arg1p->entry);
        auto r = pb(new LdVar("x", e));
        auto rf = pb(new Force(r));
        auto k = pb(new ChkMissing(rf));
        pb(new Return(k));
    }

    Promise* arg2p = f->createProm();
    {
        Builder pb(f, arg2p, e, arg2p->entry);
        auto r = pb(new LdVar("x", e));
        auto rf = pb(new Force(r));
        auto k = pb(new ChkMissing(rf));
        pb(new Return(k));
    }

    auto arg1 = b(new MkArg(arg1p, Missing::instance(), e));
    auto arg2 = b(new MkArg(arg2p, k, e));
    auto call = b(new Call(e, fun, {{arg1, arg2}}));
    auto mval = b(new Force(call));
    auto val = b(new ChkMissing(mval));

    b(new Return(val));

    Verifier v(f);
    v();

    f->print(std::cout);

    delete f;
    delete e;
}
