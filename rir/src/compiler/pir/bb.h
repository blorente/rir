#ifndef COMPILER_BB_H
#define COMPILER_BB_H

#include "pir.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace rir {
namespace pir {

class BB {
  public:
    unsigned id;

    BB(const BB&) = delete;
    void operator=(const BB&) = delete;

    BB(unsigned id);
    ~BB();

    static BB* cloneInstrs(BB* src);

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

    Instruction* last() {
        assert(instr.size() > 0);
        return instr.back();
    }

    BB* next0 = nullptr;
    BB* next1 = nullptr;

    bool jmp() { return next0 && !next1; }
    bool empty() { return instr.size() == 0; }

    typedef std::vector<Instruction*> Instrs;

    void append(Instruction* i);

    Instrs::iterator insert(Instrs::iterator it, Instruction* i);
    void replace(Instrs::iterator it, Instruction* i);

    Instrs::iterator remove(Instrs::iterator it);
    Instrs::iterator moveTo(Instrs::iterator it, BB* other);

    void print(std::ostream& = std::cout);

    Instrs instr;
};

}
}

#endif
