#include "scope.h"
#include "../pir/pir_impl.h"
#include "query.h"

namespace {
using namespace rir::pir;

class TheScopeAnalysis : public StaticAnalysisForEnvironments<AbstractValue> {
  public:
    std::unordered_map<Instruction*, AbstractLoadVal> loads;
    std::unordered_map<Value*, Function*> functions;

    TheScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysisForEnvironments(localScope, args, bb) {}
    TheScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb,
                     const A& initialState)
        : StaticAnalysisForEnvironments(localScope, args, bb, initialState) {}

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

    bool handled = false;

    if (ld) {
        loads[ld] = envs.get(ld->env(), ld->varName);
        handled = true;
    } else if (ldf) {
        loads[ldf] = envs.get(ldf->env(), ldf->varName);
        handled = true;
    } else if (lda) {
        SEXP name = args[lda->id];
        loads[lda] = envs.get(lda->env(), name);
        handled = true;
    } else if (mk) {
        Value* parentEnv = mk->env();
        envs[mk].parentEnv = parentEnv;
        mk->eachLocalVar(
            [&](SEXP name, Value* val) { envs[mk].set(name, val); });
        handled = true;
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
        if (PirType::val() >= v->type) {
            handled = true;
        }
    } else if (s) {
        envs[s->env()].set(s->varName, s->val());
        handled = true;
    } else if (call) {
        Value* trg = call->cls();
        Function* fun = envs.findFunction(i->env(), trg);
        if (fun != UnknownFunction) {
            if (envs[fun->env].parentEnv == UninitializedParent)
                envs[fun->env].parentEnv = i->env();
            TheScopeAnalysis nextFun(fun->env, fun->arg_name, fun->entry, envs);
            nextFun();
            envs.merge(nextFun.exitpoint);
            handled = true;
        }
    }

    if (!handled) {
        if (i->leaksEnv()) {
            envs[i->env()].leaked = true;
        }
        if (i->changesEnv()) {
            envs[i->env()].taint();
        }
    }

    // Keep track of closures
    MkClsFun* mkfun = MkClsFun::Cast(i);
    if (!mkfun && loads.count(i) && loads[i].second.singleValue())
        mkfun = MkClsFun::Cast(*loads[i].second.vals.begin());
    if (mkfun)
        envs[i->env()].functionPointers[i] = mkfun->fun;
}
}

namespace rir {
namespace pir {

ScopeAnalysis::ScopeAnalysis(Value* localScope, const std::vector<SEXP>& args,
                             BB* bb) {
    TheScopeAnalysis analysis(localScope, args, bb);
    analysis();
    loads = std::move(analysis.loads);
    finalState = std::move(analysis.exitpoint);
}
}
}
