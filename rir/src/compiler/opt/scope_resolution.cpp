#include "scope_resolution.h"
#include "../pir/pir_impl.h"
#include "../util/cfg.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <unordered_map>

namespace {

using namespace rir::pir;

class Argument : public Value {
  public:
    SEXP name;
    size_t id;
    Argument(SEXP name, size_t id)
        : Value(PirType::any(), Kind::argument), name(name), id(id) {}
    void printRef(std::ostream& out) {
        out << "ARG(" << CHAR(PRINTNAME(name)) << ")";
    }
};

struct AbstractValue {
    std::set<Value*> val;
    PirType type = PirType::bottom();

    bool unknown = false;

    AbstractValue() {}
    AbstractValue(Value* v) : type(v->type) { val.insert(v); }

    static AbstractValue tainted() {
        AbstractValue v;
        v.taint();
        return v;
    }

    void taint() {
        unknown = true;
        type = PirType::any();
    }

    size_t candidates() { return val.size(); }

    enum class ValKind : uint8_t { Value, Phi, Argument, Unknown };
    ValKind kind() {
        if (unknown)
            return ValKind::Unknown;
        assert(candidates() > 0);
        if (candidates() == 1) {
            return (*val.begin())->kind == Kind::argument ? ValKind::Argument
                                                          : ValKind::Value;
        }
        for (auto a : val)
            if (a->kind == Kind::argument)
                return ValKind::Unknown;
        return ValKind::Phi;
    }

    bool merge(const AbstractValue& other) {
        if (!unknown && other.unknown) {
            taint();
            return true;
        }
        if (unknown)
            return false;

        bool changed = false;
        for (auto v : other.val) {
            if (val.find(v) == val.end()) {
                type = type | v->type;
                val.insert(v);
                changed = true;
            }
        }
        return changed;
    }

    void print(std::ostream& out = std::cout) {
        if (unknown) {
            out << "??";
            return;
        }
        out << "(";
        for (auto it = val.begin(); it != val.end();) {
            (*it)->printRef(out);
            it++;
            if (it != val.end())
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
        static AbstractValue m(Missing::instance());
        if (entries.count(e))
            return entries.at(e);
        if (tainted)
            return t;
        return m;
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
    std::vector<Argument> args;
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
            args.push_back(Argument(a, id++));
            initial.set(a, &args.back());
        }

        do {
            changed = false;

            Visitor::run(function->entry, [&](BB* bb) {
                AbstractEnv env = mergepoint[bb->id];
                for (auto i : bb->instr) {
                    collect(i, env);
                    apply(env, i);
                }

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
                if (std::get<1>(i).kind() == AbstractValue::ValKind::Unknown) {
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
                    switch (v.kind()) {
                    case AbstractValue::ValKind::Value:
                        ld->replaceUsesWith(*v.val.begin());
                        if (!needEnv)
                            it = bb->remove(it);
                        break;
                    case AbstractValue::ValKind::Argument: {
                        Argument* a = static_cast<Argument*>(*v.val.begin());
                        auto lda = new LdArg(a->id, function->env);
                        ld->replaceUsesWith(lda);
                        bb->replace(it, lda);
                        break;
                    }
                    case AbstractValue::ValKind::Phi: {
                        auto phi = new Phi;
                        for (auto a : v.val) {
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
                    case AbstractValue::ValKind::Unknown:
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
