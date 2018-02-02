#include "scope.h"
#include "../pir/pir_impl.h"
#include "query.h"

namespace {
using namespace rir::pir;

class TheScopeAnalysis : public StaticAnalysisForEnvironments<AbstractValue> {
  public:
    static constexpr size_t maxDepth = 5;
    size_t depth;
    Call* invocation = nullptr;

    std::unordered_map<Instruction*, AbstractLoadVal> loads;
    std::unordered_map<Value*, Function*> functions;

    TheScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysisForEnvironments(localScope, args, bb), depth(0) {}
    TheScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb,
                     const A& initialState, Call* invocation, size_t depth)
        : StaticAnalysisForEnvironments(localScope, args, bb, initialState),
          depth(depth), invocation(invocation) {}

    void apply(A& envs, Instruction* i) override;
    void init(A& initial) override {
        if (invocation) {
            E& env = initial[localScope];
            if (args.size() == invocation->nCallArgs()) {
                for (size_t i = 0; i < invocation->nCallArgs(); ++i) {
                    env.set(args[i], invocation->callArgs()[i]);
                }
                return;
            }
        }
        StaticAnalysisForEnvironments::init(initial);
    }
};

void TheScopeAnalysis::apply(A& envs, Instruction* i) {
    StVar* s = StVar::Cast(i);
    LdVar* ld = LdVar::Cast(i);
    LdArg* lda = LdArg::Cast(i);
    LdFun* ldf = LdFun::Cast(i);
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
    } else if (s) {
        envs[s->env()].set(s->varName, s->val());
        handled = true;
    } else if (call && depth < maxDepth) {
        Value* trg = call->cls();
        Function* fun = envs.findFunction(i->env(), trg);
        if (fun != UnknownFunction) {
            if (envs[fun->env].parentEnv == UninitializedParent)
                envs[fun->env].parentEnv = i->env();
            TheScopeAnalysis nextFun(fun->env, fun->arg_name, fun->entry, envs,
                                     call, depth + 1);
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
