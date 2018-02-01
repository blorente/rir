#ifndef PIR_SCOPE_ANALYSIS_H
#define PIR_SCOPE_ANALYSIS_H

#include "../analysis/generic_static_analysis.h"
#include "../pir/pir.h"
#include "abstract_value.h"

#include <algorithm>
#include <set>
#include <unordered_map>

namespace rir {
namespace pir {

class ScopeAnalysis {
    typedef StaticAnalysisForEnvironments<AbstractValue> Instance;
  public:
    std::unordered_map<Instruction*, Instance::AbstractLoadVal> loads;
    Instance::A finalState;
    ScopeAnalysis(Value* localScope, const std::vector<SEXP>& args, BB* bb);
};
}
}

#endif
