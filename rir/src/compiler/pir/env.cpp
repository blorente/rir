#include "env.h"

namespace rir {
namespace pir {

size_t Env::envIdCount = 1;

void Env::printRef(std::ostream& out) { out << "env_" << envId; }
}
}
