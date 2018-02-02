#ifndef BB_TRANSFORM_H
#define BB_TRANSFORM_H

#include "../pir/bb.h"
#include "../pir/pir.h"

namespace rir {
namespace pir {

class BBTransform {
  public:
    static BB* clone(size_t* id_counter, BB* src);
    static BB* clone(BB* src) {
        size_t c = 1;
        return clone(&c, src);
    }
    static BB* split(size_t next_id, BB* src, BB::Instrs::iterator);
    static Value* forInline(BB* inlinee, BB* cont);
};
}
}

#endif
