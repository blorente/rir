#include "query.h"
#include "../pir/pir_impl.h"
#include "../util/visitor.h"

namespace rir {
namespace pir {

bool Query::pure(Code* c) {
    return Visitor::check(c->entry, [](BB* bb) {
        for (auto i : bb->instr)
            if (i->mightIO() || i->changesEnv())
                return false;
        return true;
    });
}
bool Query::noUnknownEnvAccess(Code* c, Env* e) {
    return Visitor::check(c->entry, [&](BB* bb) {
        for (auto i : bb->instr) {
            LdArg* ld = LdArg::Cast(i);
            if (ld && ld->env() == e) {
            } else if (i->needsEnv()) {
                return false;
            }
        }
        return true;
    });
}
}
}
