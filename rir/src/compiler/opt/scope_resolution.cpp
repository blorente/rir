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
    enum class AKind : unsigned { Bottom, Values, Arguments, Unknown };

    std::set<Value*> vals;
    std::set<size_t> args;

    PirType type = PirType::bottom();

    AKind kind = AKind::Bottom;

    AbstractValue(PirType t = PirType::bottom()) : type(t) {}
    AbstractValue(Value* v) : type(v->type), kind(AKind::Values) {
        vals.insert(v);
    }

    static AbstractValue arg(size_t id) {
        AbstractValue v(PirType::any());
        v.kind = AKind::Arguments;
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
        kind = AKind::Unknown;
        type = PirType::any();
    }

    bool isUnknown() { return kind == AKind::Unknown; }

    size_t candidates() {
        assert(kind != AKind::Unknown && kind != AKind::Bottom);
        if (kind == AKind::Values)
            return vals.size();
        if (kind == AKind::Arguments)
            return args.size();
        assert(false);
        return 0;
    }

    static constexpr unsigned cross(AKind a, AKind b) {
        return ((unsigned)a << 4) + (unsigned)b;
    }

    bool merge(const AbstractValue& other) {
        assert(other.kind != AKind::Bottom);

        if (kind == AKind::Unknown)
            return false;
        if (kind == AKind::Bottom) {
            *this = other;
            return true;
        }
        if (other.kind == AKind::Unknown) {
            taint();
            return true;
        }
        switch (cross(kind, other.kind)) {
        case cross(AKind::Arguments, AKind::Values):
        case cross(AKind::Values, AKind::Arguments):
            taint();
            return true;
        case cross(AKind::Arguments, AKind::Arguments):
            if (std::includes(args.begin(), args.end(), other.args.begin(),
                              other.args.end()))
                return false;
            args.insert(other.args.begin(), other.args.end());
            return true;
        case cross(AKind::Values, AKind::Values):
            if (std::includes(vals.begin(), vals.end(), other.vals.begin(),
                              other.vals.end()))
                return false;
            vals.insert(other.vals.begin(), other.vals.end());
            return true;
        default:
            assert(false);
        }
        assert(false);
        return false;
    }

    void print(std::ostream& out = std::cout) {
        assert(kind != AKind::Bottom);
        if (kind == AKind::Unknown) {
            out << "??";
            return;
        }
        if (kind == AKind::Values) {
            out << "val(";
            for (auto it = vals.begin(); it != vals.end();) {
                (*it)->printRef(out);
                it++;
                if (it != vals.end())
                    out << "|";
            }
        } else {
            assert(kind == AKind::Arguments);
            out << "arg(";
            for (auto it = args.begin(); it != args.end();) {
                std::cout << *it;
                it++;
                if (it != args.end())
                    out << ",";
            }
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

    const AbstractValue& get(SEXP e) {
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
        std::unordered_map<LdVar*, AbstractValue> loads;

        ScopeAnalysis a(function);
        a([&](Instruction* i, AbstractEnv& env) {
            LdVar* ld;
            if ((ld = LdVar::Cast(i))) {
                loads[ld] = env.get(ld->varName);
            }
        });

        bool needEnv = a.exitpoint.leaked;
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
                LdVar* ld;
                if (!needEnv && StVar::Cast(i)) {
                    it = bb->remove(it);
                } else if ((ld = LdVar::Cast(i))) {
                    auto v = loads[ld];
                    switch (v.kind) {
                    case AbstractValue::AKind::Values: {
                        if (v.candidates() == 1) {
                            ld->replaceUsesWith(*v.vals.begin());
                            if (!needEnv)
                                it = bb->remove(it);
                            break;
                        }
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
                        break;
                    }
                    case AbstractValue::AKind::Arguments: {
                        if (v.candidates() == 1) {
                            size_t arg = *v.args.begin();
                            auto lda = new LdArg(arg, function->env);
                            ld->replaceUsesWith(lda);
                            bb->replace(it, lda);
                            break;
                        }
                        // TODO
                        break;
                    }
                    case AbstractValue::AKind::Bottom:
                        assert(false);
                    case AbstractValue::AKind::Unknown:
                        break;
                    }
                };
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
