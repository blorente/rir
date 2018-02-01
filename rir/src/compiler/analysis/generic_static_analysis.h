#ifndef PIR_GENERIC_STATIC_ANALYSIS
#define PIR_GENERIC_STATIC_ANALYSIS

#include "../pir/bb.h"
#include "../pir/function.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <functional>
#include <set>
#include <unordered_map>

namespace rir {
namespace pir {

static Value* UnknownParent = (Value*)-1;
static Value* UninitializedParent = nullptr;
static Function* UnknownFunction = (Function*)-1;

template <class AbstractValue>
struct AbstractEnvironment {

    std::unordered_map<SEXP, AbstractValue> entries;
    std::unordered_map<Value*, Function*> functionPointers;

    Value* parentEnv = UninitializedParent;

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

    bool merge(AbstractEnvironment& other) {
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
            if (entries[n].merge(other.get(n)))
                changed = true;
        }

        std::set<Value*> fps;
        for (auto e : functionPointers)
            fps.insert(std::get<0>(e));
        for (auto e : other.functionPointers)
            fps.insert(std::get<0>(e));
        for (auto n : fps) {
            if (functionPointers[n] != UnknownFunction &&
                functionPointers[n] != other.functionPointers[n]) {
                functionPointers[n] = UnknownFunction;
            }
        }

        if (parentEnv == UninitializedParent &&
            other.parentEnv != UninitializedParent) {
            parentEnv = other.parentEnv;
            changed = true;
        }
        return changed;
    }
};

template <class AbstractState>
class StaticAnalysis {
    std::vector<AbstractState> mergepoint;

  public:
    AbstractState exitpoint;
    BB* entry;

    StaticAnalysis(BB* entry) : entry(entry) {}
    StaticAnalysis(BB* entry, const AbstractState& initialState)
        : entry(entry) {
        mergepoint.resize(entry->id + 1);
        mergepoint[entry->id] = initialState;
    }

    virtual void apply(AbstractState&, Instruction*) = 0;
    virtual void init(AbstractState&) = 0;

    void operator()() {
        mergepoint.resize(entry->id + 5);
        bool changed;

        init(mergepoint[entry->id]);

        do {
            changed = false;

            Visitor::run(entry, [&](BB* bb) {
                // TODO: here we dynamically grow the size, it's a bit of a
                // hack...
                size_t max_id = bb->id;
                if (bb->next0 && bb->next0->id > max_id)
                    max_id = bb->next0->id;
                if (bb->next1 && bb->next1->id > max_id)
                    max_id = bb->next1->id;
                if (max_id >= mergepoint.size())
                    mergepoint.resize(max_id + 1);

                AbstractState state = mergepoint[bb->id];
                for (auto i : *bb) {
                    apply(state, i);
                }

                if (!bb->next0 && !bb->next1) {
                    exitpoint.merge(state);
                    return;
                }

                if (bb->next0) {
                    changed = mergepoint[bb->next0->id].merge(state) || changed;
                }
                if (bb->next1) {
                    changed = mergepoint[bb->next1->id].merge(state) || changed;
                }
            });
        } while (changed);
        mergepoint.clear();
    }
};

template <class AbstractValue>
class AbstractEnvironmentSet
    : public std::unordered_map<Value*, AbstractEnvironment<AbstractValue>> {
  public:
    typedef std::pair<Value*, AbstractValue> AbstractLoadVal;
    bool merge(AbstractEnvironmentSet& other) {
        bool changed = false;
        std::set<Value*> k;
        for (auto e : *this)
            k.insert(e.first);
        for (auto e : other)
            k.insert(e.first);
        for (auto i : k)
            if ((*this)[i].merge(other[i]))
                changed = true;
        return changed;
    }
    void clear() {
        for (auto e : *this)
            e.second.clear();
    }

    Function* findFunction(Value* env, Value* fun) {
        while (env != UnknownParent) {
            assert(env);
            if ((*this)[env].functionPointers.count(fun))
                return (*this)[env].functionPointers.at(fun);
            env = (*this)[env].parentEnv;
        }
        return UnknownFunction;
    }

    AbstractLoadVal get(Value* env, SEXP e) {
        while (env != UnknownParent) {
            if (this->count(env) == 0)
                return AbstractLoadVal(env, AbstractValue::tainted());
            const AbstractValue& res = (*this)[env].get(e);
            if (!res.isUnknown())
                return AbstractLoadVal(env, res);
            env = (*this)[env].parentEnv;
        }
        return AbstractLoadVal(env, AbstractValue::tainted());
    }
};

template <class AbstractValue>
class StaticAnalysisForEnvironments
    : public StaticAnalysis<AbstractEnvironmentSet<AbstractValue>> {
  public:
    typedef AbstractEnvironmentSet<AbstractValue> A;
    typedef AbstractEnvironment<AbstractValue> E;
    typedef std::pair<Value*, AbstractValue> AbstractLoadVal;

    const std::vector<SEXP>& args;

    Value* localScope;

    StaticAnalysisForEnvironments(Value* localScope,
                                  const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysis<A>(bb), args(args), localScope(localScope) {}

    StaticAnalysisForEnvironments(Value* localScope,
                                  const std::vector<SEXP>& args, BB* bb,
                                  const A& initialState)
        : StaticAnalysis<A>(bb, initialState), args(args),
          localScope(localScope) {}

    void init(A& initial) override {
        E& env = initial[localScope];
        size_t id = 0;
        for (auto a : args) {
            env.set_arg(a, id++);
        }
        env.parentEnv = UnknownParent;
    }
};
}
}

#endif
