#include "cfg.h"
#include "../pir/pir_impl.h"
#include "../util/visitor.h"

namespace rir {
namespace pir {

CFG::CFG(BB* start) {
    size_t max = 0;
    Visitor::run(start, [&](BB* bb) {
        if (bb->id > max)
            max = bb->id;
    });
    preds_.resize(max + 1);
    Visitor::run(start, [&](BB* bb) {
        if (bb->next0)
            preds_[bb->next0->id].push_back(bb);
        if (bb->next1)
            preds_[bb->next1->id].push_back(bb);
    });
}

CFG::Preds CFG::preds(BB* bb) { return preds_[bb->id]; }
}
}
