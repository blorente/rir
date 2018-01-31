#include "scope.h"
#include "../pir/pir_impl.h"
#include "query.h"

namespace rir {
namespace pir {

AbstractValue::AbstractValue(Value* v) : type(v->type) { vals.insert(v); }

bool AbstractValue::merge(const AbstractValue& other) {
    assert(other.type != PirType::bottom());

    if (unknown)
        return false;
    if (type == PirType::bottom()) {
        *this = other;
        return true;
    }
    if (other.unknown) {
        taint();
        return true;
    }

    bool changed = false;
    if (!std::includes(vals.begin(), vals.end(), other.vals.begin(),
                       other.vals.end())) {
        vals.insert(other.vals.begin(), other.vals.end());
        changed = true;
    }
    if (!std::includes(args.begin(), args.end(), other.args.begin(),
                       other.args.end())) {
        args.insert(other.args.begin(), other.args.end());
        changed = true;
    }

    return changed;
}

void AbstractValue::print(std::ostream& out) {
    if (unknown) {
        out << "??";
        return;
    }
    out << "(";
    for (auto it = vals.begin(); it != vals.end();) {
        (*it)->printRef(out);
        it++;
        if (it != vals.end() && args.size() != 0)
            out << "|";
    }
    for (auto it = args.begin(); it != args.end();) {
        out << *it;
        it++;
        if (it != args.end())
            out << "|";
    }
    out << ") : " << type;
}

void ScopeAnalysis::apply(A& envs, Instruction* i) {
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
            Function* fun = mkfun->fun;
            if (Query::noUnknownEnvAccess(fun, fun->env))
                return;
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
}
