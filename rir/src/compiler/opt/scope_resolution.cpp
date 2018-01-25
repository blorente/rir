#include "scope_resolution.h"
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

struct AbstractEnv {
    std::unordered_map<SEXP, AbstractValue> entries;

    bool leaked = false;
    bool tainted = false;

    void taint() {
        tainted = true;
        for (auto& e : entries) {
            e.second.taint();
        }
    }

    void set(SEXP n, Value* v) { entries[n] = AbstractValue(v); }
    void set_arg(SEXP n, size_t id) { entries[n] = AbstractValue::arg(id); }

    void print(std::ostream& out = std::cout) {
        for (auto e : entries) {
            SEXP name = std::get<0>(e);
            out << "   " << CHAR(PRINTNAME(name)) << " -> ";
            AbstractValue v = std::get<1>(e);
            v.print(out);
            out << "\n";
        }
        out << "\n";
    }

    AbstractValue& get(SEXP e) {
        static AbstractValue t = AbstractValue::tainted();
        if (entries.count(e))
            return entries.at(e);
        return t;
    }

    bool merge(AbstractEnv& other) {
        bool changed = false;
        if (!leaked && other.leaked)
            changed = leaked = true;
        if (!tainted && other.tainted)
            changed = tainted = true;
        std::set<SEXP> keys;
        for (auto e : entries)
            keys.insert(std::get<0>(e));
        for (auto e : other.entries)
            keys.insert(std::get<0>(e));
        for (auto n : keys) {
            changed = changed || entries[n].merge(other.get(n));
        }
        return changed;
    }
};

class ScopeAnalysis {
  public:
    std::vector<AbstractEnv> mergepoint;
    AbstractEnv exitpoint;
    Function* function;
    ScopeAnalysis(Function* function) : function(function) {}
    typedef std::function<void(Instruction* i, AbstractEnv& env)>
        collect_result;

    void operator()(collect_result collect) {
        CFG cfg(function->entry);

        mergepoint.resize(cfg.size());
        bool changed;

        // Insert arguments into environment
        AbstractEnv& initial = mergepoint[function->entry->id];
        size_t id = 0;
        for (auto a : function->arg_name) {
            initial.set_arg(a, id++);
        }

        do {
            changed = false;

            Visitor::run(function->entry, [&](BB* bb) {
                AbstractEnv env = mergepoint[bb->id];
                for (auto i : bb->instr) {
                    collect(i, env);
                    apply(env, i);
                }

                // std::cout << "After " << bb->id << ":\n";
                // for (auto i : env.entries) {
                //     std::cout << CHAR(PRINTNAME(std::get<0>(i))) << " -> ";
                //     std::get<1>(i).print(std::cout);
                //     std::cout << "\n";
                // }

                if (!bb->next0 && !bb->next1) {
                    exitpoint.merge(env);
                    return;
                }

                if (bb->next0)
                    changed = changed || mergepoint[bb->next0->id].merge(env);
                if (bb->next1)
                    changed = changed || mergepoint[bb->next1->id].merge(env);
            });
        } while (changed);
    }

    static void apply(AbstractEnv& env, Instruction* i) {
        StVar* s;

        if ((s = StVar::Cast(i))) {
            env.set(s->varName, s->val());
            return;
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
        std::unordered_map<Instruction*, AbstractValue> loads;

        ScopeAnalysis analyze(function);
        analyze([&](Instruction* i, AbstractEnv& env) {
            LdVar* ld = LdVar::Cast(i);
            LdFun* ldf = LdFun::Cast(i);
            if (ld)
                loads[ld] = env.get(ld->varName);
            else if (ldf)
                loads[ldf] = env.get(ldf->varName);
        });

        bool needEnv = analyze.exitpoint.leaked;
        if (!needEnv) {
            for (auto i : loads) {
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
                    auto v = loads[ld];
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
