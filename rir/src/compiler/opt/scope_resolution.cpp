#include "scope_resolution.h"
#include "../analysis/generic_static_analysis.h"
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

    bool isUnknown() { return unknown; }

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

    bool merge(AbstractValue& other) {
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
    std::unordered_map<Instruction*, AbstractValue> loads;

    ScopeAnalysis(const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysisForEnvironments(args, bb) {}

    void apply(AbstractEnv& env, Instruction* i) override {
        StVar* s = StVar::Cast(i);
        LdVar* ld = LdVar::Cast(i);
        LdFun* ldf = LdFun::Cast(i);
        Force* force = Force::Cast(i);

        if (ld) {
            loads[ld] = env.get(ld->varName);
        } else if (ldf) {
            loads[ldf] = env.get(ldf->varName);
        } else if (s) {
            env.set(s->varName, s->val());
            return;
        } else if (force) {
            Value* v = force->arg<0>();
            ld = LdVar::Cast(v);
            if (ld) {
                if (env.get(ld->varName).isUnknown())
                    env.set(ld->varName, force);
            }
        }

        if (!env.leaked && i->leaksEnv()) {
            env.leaked = true;
        }
        if (env.leaked && i->changesEnv()) {
            env.taint();
        }
    }
};

class TheScopeResolution {
  public:
    Function* function;
    TheScopeResolution(Function* function) : function(function) {}
    void operator()() {
        ScopeAnalysis analysis(function->arg_name, function->entry);
        analysis();

        bool needEnv = analysis.exitpoint.leaked;
        if (!needEnv) {
            for (auto i : analysis.loads) {
                if (std::get<1>(i).isUnknown()) {
                    needEnv = true;
                    break;
                }
            }
        }

        Visitor::run(function->entry, [&](BB* bb) {
            for (auto it = bb->instr.begin(); it != bb->instr.end(); it++) {
                Instruction* i = *it;
                LdVar* lda = LdVar::Cast(i);
                LdFun* ldf = LdFun::Cast(i);
                Instruction* ld = nullptr;
                if (lda)
                    ld = lda;
                else if (ldf)
                    ld = ldf;
                if (!needEnv && StVar::Cast(i)) {
                    it = bb->remove(it);
                } else if (ld) {
                    auto v = analysis.loads[ld];
                    if (v.singleValue()) {
                        Value* val = *v.vals.begin();
                        ld->replaceUsesWith(val);
                        if (!ld->changesEnv() || !val->type.maybeLazy())
                            it = bb->remove(it);
                    } else if (v.singleArg()) {
                        auto lda = new LdArg(*v.args.begin());
                        ld->replaceUsesWith(lda);
                        bb->replace(it, lda);
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
                            it++;
                            it = bb->insert(it, phi);
                        } else {
                            bb->replace(it, phi);
                        }
                    }
                }
                if (it == bb->instr.end())
                    return;
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
