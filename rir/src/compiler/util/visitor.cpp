#include "visitor.h"
#include "../pir/pir_impl.h"

#include <deque>

namespace {
using namespace rir::pir;

enum class Traversal {
    DepthFirst,
    BreadthFirst,
};

template <Traversal TRAVERSAL>
struct BBVisitor {
    std::deque<BB*> todo;
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
        done.insert(cur);

        while (cur) {
            BB* next = nullptr;

            if (cur->next0 && done.find(cur->next0) == done.end()) {
                if (TRAVERSAL == Traversal::DepthFirst || todo.empty())
                    next = cur->next0;
                else
                    todo.push_front(cur->next0);
                done.insert(cur->next0);
            }

            if (cur->next1 && done.find(cur->next1) == done.end()) {
                if (!next &&
                    (TRAVERSAL == Traversal::DepthFirst || todo.empty())) {
                    next = cur->next1;
                } else {
                    if (TRAVERSAL == Traversal::DepthFirst)
                        todo.push_back(cur->next1);
                    else
                        todo.push_front(cur->next1);
                }
                done.insert(cur->next1);
            }

            if (!next) {
                if (!todo.empty()) {
                    next = todo.back();
                    todo.pop_back();
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
    BBVisitor<Traversal::DepthFirst> v(action);
    return v(bb);
}

void Visitor::run(BB* bb, BBAction action) {
    BBVisitor<Traversal::BreadthFirst> v([&](BB* bb) {
        action(bb);
        return true;
    });
    v(bb);
}
}
}
