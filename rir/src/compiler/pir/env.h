#ifndef COMPILER_ENV_H
#define COMPILER_ENV_H

#include <iostream>

namespace rir {
namespace pir {

class Env {
    friend std::ostream& operator<<(std::ostream& out, const Env& e) {
        out << "Env(" << (void*)&e << ")";
        return out;
    }
};
}
}

#endif
