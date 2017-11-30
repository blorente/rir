#ifndef COMPILER_VISITOR_H
#define COMPILER_VISITOR_H

#include "bb.h"

#include <set>

namespace rir {
namespace pir {

template <class Receiver>
class Visitor {
    Receiver* r;
    BB* cur;

    std::set<BB*> todo;
    std::set<BB*> done;

  public:
    Visitor(Receiver* r) : r(r) {}

    void operator()(BB* start) {
        assert(todo.empty());
        todo.clear();
        done.clear();

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

            r->accept(cur);

            cur = next;
        }
    }
};
}
}

#endif
