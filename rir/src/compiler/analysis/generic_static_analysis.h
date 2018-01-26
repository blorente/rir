#ifndef PIR_GENERIC_STATIC_ANALYSIS
#define PIR_GENERIC_STATIC_ANALYSIS

#include "../pir/bb.h"
#include "../pir/function.h"
#include "../pir/pir.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <functional>
#include <set>
#include <unordered_map>

namespace rir {
namespace pir {

template <class AbstractValue>
struct AbstractEnvironment {
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
            changed = changed || entries[n].merge(other.get(n));
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

    virtual void apply(AbstractState& env, Instruction* i) = 0;
    virtual void init(AbstractState& env) = 0;

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
                for (auto i : bb->instr) {
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
class StaticAnalysisForEnvironments
    : public StaticAnalysis<AbstractEnvironment<AbstractValue>> {
  public:
    typedef AbstractEnvironment<AbstractValue> AbstractEnv;

    const std::vector<SEXP>& args;

    StaticAnalysisForEnvironments(const std::vector<SEXP>& args, BB* bb)
        : StaticAnalysis<AbstractEnv>(bb), args(args) {}

    void init(AbstractEnv& initial) override {
        size_t id = 0;
        for (auto a : args) {
            initial.set_arg(a, id++);
        }
    }
};
}
}

#endif
