#ifndef COMPILER_BB_H
#define COMPILER_BB_H

#include "instruction.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace rir {
namespace pir {

class BB {
  public:
    const unsigned id;

    BB(const BB&) = delete;
    void operator=(const BB&) = delete;

    BB(unsigned id);
    ~BB() {
        for (auto* i : instr)
            delete i;
    }

    unsigned indexOf(Instruction* i) {
        unsigned p = 0;
        for (auto j : instr) {
            if (i == j)
                return p;
            p++;
        }
        assert(false);
        return 0;
    }

    Instruction* last() { return instr.back(); }

    std::vector<Instruction*> instr;
    BB* next0 = nullptr;
    BB* next1 = nullptr;

    void print(std::ostream& = std::cout);

    BB* createNext(unsigned id) {
        next0 = new BB(id);
        return next0;
    }
};

inline BB& operator<<(BB& bb, Instruction* i) {
    i->bb_ = &bb;
    bb.instr.push_back(i);
    return bb;
}
}
}

#endif
