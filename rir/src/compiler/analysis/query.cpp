#include "query.h"
#include "../pir/pir_impl.h"
#include "../util/visitor.h"

namespace rir {
namespace pir {

bool Query::pure(Code* c) {
    return Visitor::check(c->entry, [](BB* bb) {
        for (auto i : *bb)
            if (i->mightIO() || i->changesEnv())
                return false;
        return true;
    });
}

bool Query::doesNotNeedEnv(Code* c) {
    return Visitor::check(c->entry, [&](BB* bb) {
        for (auto i : *bb) {
            LdVar* ld = LdVar::Cast(i);
            LdFun* ldf = LdFun::Cast(i);
            if (ld && ldf) {
                return false;
            } else if (i->leaksEnv()) {
                return false;
            }
        }
        return true;
    });
}
}
}
