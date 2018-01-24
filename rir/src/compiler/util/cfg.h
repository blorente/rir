#ifndef PIR_CFG_H
#define PIR_CFG_H

#include "../pir/pir.h"
#include <vector>

namespace rir {
namespace pir {

class CFG {
    typedef std::vector<BB*> Preds;
    std::vector<Preds> preds_;

  public:
    size_t size() { return preds_.size(); }
    CFG(BB*);
    Preds preds(BB*);
};
}
}

#endif
