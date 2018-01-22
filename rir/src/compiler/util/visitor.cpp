#include "visitor.h"
#include "../pir/pir_impl.h"

namespace {
using namespace rir::pir;

struct BBVisitor {
    std::set<BB*> todo;
    std::set<BB*> done;
    BB* cur;
    Visitor::BBReturnAction action;

    BBVisitor(Visitor::BBReturnAction action) : action(action) {}

    void clear() {
        todo.clear();
        done.clear();
    }

    bool operator()(BB* start) {
        assert(todo.empty());
        cur = start;

        while (cur) {
            done.insert(cur);

            BB* next = nullptr;

            if (cur->next0 && done.find(cur->next0) == done.end())
                next = cur->next0;

            if (cur->next1 && done.find(cur->next1) == done.end()) {
                if (!next)
                    next = cur->next1;
                else
                    todo.insert(cur->next1);
            }

            if (!next) {
                auto c = todo.begin();
                if (c != todo.end()) {
                    next = *c;
                    todo.erase(c);
                }
            }

            if (!action(cur)) {
                clear();
                return false;
            }

            cur = next;
        }
        return true;
    }
};
}

namespace rir {
namespace pir {

bool Visitor::check(BB* bb, BBReturnAction action) {
    BBVisitor v(action);
    return v(bb);
}

void Visitor::run(BB* bb, BBAction action) {
    check(bb, [&](BB* bb) -> bool {
        action(bb);
        return true;
    });
}
}
}
