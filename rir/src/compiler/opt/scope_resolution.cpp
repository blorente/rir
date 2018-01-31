#include "scope_resolution.h"
#include "../analysis/generic_static_analysis.h"
#include "../analysis/query.h"
#include "../pir/pir_impl.h"
#include "../util/cfg.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <algorithm>
#include <unordered_map>

namespace {

using namespace rir::pir;

struct AbstractValue {
    bool unknown = false;

    std::set<Value*> vals;
    std::set<size_t> args;

    PirType type = PirType::bottom();

    AbstractValue(PirType t = PirType::bottom()) : type(t) {}
    AbstractValue(Value* v) : type(v->type) { vals.insert(v); }

    static AbstractValue arg(size_t id) {
        AbstractValue v(PirType::any());
        v.args.insert(id);
        return v;
    }

    static AbstractValue tainted() {
        AbstractValue v(PirType::any());
        v.taint();
        return v;
    }

    void taint() {
        vals.clear();
        args.clear();
        unknown = true;
        type = PirType::any();
    }

    bool isUnknown() const { return unknown; }

    bool singleValue() {
        if (unknown)
            return false;
        return vals.size() == 1 && args.size() == 0;
    }
    bool singleArg() {
        if (unknown)
            return false;
        return args.size() == 1 && vals.size() == 0;
    }

    bool merge(const AbstractValue& other) {
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

    void print(std::ostream& out = std::cout) {
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
};

class ScopeAnalysis : public StaticAnalysisForEnvironments<AbstractValue> {
  public:
    std::unordered_map<Instruction*, AbstractLoadVal> loads;

    ScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysisForEnvironments(localScope, args, bb) {}

    void apply(A& envs, Instruction* i) override {
        StVar* s = StVar::Cast(i);
        LdVar* ld = LdVar::Cast(i);
        LdArg* lda = LdArg::Cast(i);
        LdFun* ldf = LdFun::Cast(i);
        Force* force = Force::Cast(i);
        MkEnv* mk = MkEnv::Cast(i);

        if (ld) {
            loads[ld] = envs.get(ld->env(), ld->varName);
        } else if (ldf) {
            loads[ldf] = envs.get(ldf->env(), ldf->varName);
        } else if (lda) {
            SEXP name = args[lda->id];
            loads[lda] = envs.get(lda->env(), name);
        } else if (s) {
            envs[s->env()].set(s->varName, s->val());
            return;
        } else if (mk) {
            Value* parentEnv = mk->env();
            envs[mk].parentEnv = parentEnv;
            mk->eachLocalVar(
                [&](SEXP name, Value* val) { envs[mk].set(name, val); });
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
        }

        if (i->leaksEnv()) {
            envs[i->env()].leaked = true;
        }
        if (i->changesEnv()) {
            envs[i->env()].taint();
        }
    }
};

class TheScopeResolution {
  public:
    Function* function;
    TheScopeResolution(Function* function) : function(function) {}
    void operator()() {
        ScopeAnalysis analysis(function->env, function->arg_name,
                               function->entry);
        analysis();

        bool needEnv = analysis.exitpoint[function->env].leaked;
        if (!needEnv) {
            needEnv = Query::noUnknownEnvAccess(function, function->env);
        }

        Visitor::run(function->entry, [&](BB* bb) {
            auto ip = bb->begin();
            while (ip != bb->end()) {
                Instruction* i = *ip;
                auto next = ip + 1;
                LdArg* lda = LdArg::Cast(i);
                LdFun* ldf = LdFun::Cast(i);
                Instruction* ld = LdVar::Cast(i);
                if (lda)
                    ld = lda;
                else if (ldf)
                    ld = ldf;
                if (!needEnv && StVar::Cast(i)) {
                    next = bb->remove(ip);
                } else if (ld) {
                    auto aload = analysis.loads[ld];
                    auto env = aload.first;
                    auto v = aload.second;
                    if (v.singleValue()) {
                        Value* val = *v.vals.begin();
                        ld->replaceUsesWith(val);
                        if (!ld->changesEnv() || !val->type.maybeLazy())
                            next = bb->remove(ip);
                    } else if (v.singleArg()) {
                        auto lda = new LdArg(*v.args.begin(), env);
                        ld->replaceUsesWith(lda);
                        bb->replace(ip, lda);
                    } else if (!v.vals.empty() && v.args.empty()) {
                        // TODO: mixing args and vals, but placing the LdArgs is
                        // hard...
                        auto phi = new Phi;
                        for (auto a : v.vals) {
                            phi->push_arg(a);
                        }
                        phi->updateType();
                        ld->replaceUsesWith(phi);
                        if (needEnv) {
                            ip++;
                            next = bb->insert(ip, phi);
                        } else {
                            bb->replace(ip, phi);
                        }
                    }
                }
                ip = next;
            }
        });
    }
};
}

namespace rir {
namespace pir {

void ScopeResolution::apply(Function* function) {
    TheScopeResolution s(function);
    s();
}
}
}
