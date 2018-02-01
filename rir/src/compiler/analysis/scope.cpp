#include "scope.h"
#include "../pir/pir_impl.h"
#include "query.h"

namespace {
using namespace rir::pir;

class TheScopeAnalysis : public StaticAnalysisForEnvironments<AbstractValue> {
  public:
    std::unordered_map<Instruction*, AbstractLoadVal> loads;

    TheScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysisForEnvironments(localScope, args, bb) {}

    void apply(A& envs, Instruction* i) override;
};

void TheScopeAnalysis::apply(A& envs, Instruction* i) {
    StVar* s = StVar::Cast(i);
    LdVar* ld = LdVar::Cast(i);
    LdArg* lda = LdArg::Cast(i);
    LdFun* ldf = LdFun::Cast(i);
    Force* force = Force::Cast(i);
    MkEnv* mk = MkEnv::Cast(i);
    Call* call = Call::Cast(i);

    if (ld) {
        loads[ld] = envs.get(ld->env(), ld->varName);
        return;
    } else if (ldf) {
        loads[ldf] = envs.get(ldf->env(), ldf->varName);
        return;
    } else if (lda) {
        SEXP name = args[lda->id];
        loads[lda] = envs.get(lda->env(), name);
        return;
    } else if (mk) {
        Value* parentEnv = mk->env();
        envs[mk].parentEnv = parentEnv;
        mk->eachLocalVar(
            [&](SEXP name, Value* val) { envs[mk].set(name, val); });
        return;
    } else if (force) {
        Value* v = force->arg<0>();
        ld = LdVar::Cast(v);
        lda = LdArg::Cast(v);
        if (ld) {
            if (!envs[ld->env()].get(ld->varName).isUnknown())
                envs[ld->env()].set(ld->varName, force);
        } else if (lda) {
            SEXP name = args[lda->id];
            if (!envs[lda->env()].get(name).isUnknown())
                envs[lda->env()].set(name, force);
        }
        if (PirType::val() >= v->type)
            return;
    } else if (s) {
        envs[s->env()].set(s->varName, s->val());
        return;
    } else if (call) {
        Value* trg = call->cls();
        MkClsFun* mkfun = MkClsFun::Cast(trg);
        if (mkfun) {
            //            Function* fun = mkfun->fun;
        }
    }

    if (i->leaksEnv()) {
        envs[i->env()].leaked = true;
    }
    if (i->changesEnv()) {
        envs[i->env()].taint();
    }
}
}

namespace rir {
namespace pir {

ScopeAnalysis::ScopeAnalysis(Value* localScope, const std::vector<SEXP>& args,
                             BB* bb) {
    TheScopeAnalysis analysis(localScope, args, bb);
    analysis();
    loads = analysis.loads;
    finalState = analysis.exitpoint;
}
}
}
