#include "scope_resolution.h"
#include "../analysis/query.h"
#include "../analysis/scope.h"
#include "../pir/pir_impl.h"
#include "../util/cfg.h"
#include "../util/visitor.h"
#include "R/r.h"

#include <algorithm>
#include <unordered_map>

namespace {

using namespace rir::pir;
class TheScopeResolution {
  public:
    Function* function;
    TheScopeResolution(Function* function) : function(function) {}
    void operator()() {
        ScopeAnalysis analysis(function->env, function->arg_name,
                               function->entry);
        analysis();

        bool needEnv = analysis.exitpoint[function->env].leaked;
        if (!needEnv) {
            needEnv = Query::noUnknownEnvAccess(function, function->env);
        }

        Visitor::run(function->entry, [&](BB* bb) {
            auto ip = bb->begin();
            while (ip != bb->end()) {
                Instruction* i = *ip;
                auto next = ip + 1;
                LdArg* lda = LdArg::Cast(i);
                LdFun* ldf = LdFun::Cast(i);
                Instruction* ld = LdVar::Cast(i);
                if (lda)
                    ld = lda;
                else if (ldf)
                    ld = ldf;
                if (!needEnv && StVar::Cast(i)) {
                    next = bb->remove(ip);
                } else if (ld) {
                    auto aload = analysis.loads[ld];
                    auto env = aload.first;
                    auto v = aload.second;
                    if (v.singleValue()) {
                        Value* val = *v.vals.begin();
                        ld->replaceUsesWith(val);
                        if (!ld->changesEnv() || !val->type.maybeLazy())
                            next = bb->remove(ip);
                    } else if (v.singleArg()) {
                        auto lda = new LdArg(*v.args.begin(), env);
                        ld->replaceUsesWith(lda);
                        bb->replace(ip, lda);
                    } else if (!v.vals.empty() && v.args.empty()) {
                        // TODO: mixing args and vals, but placing the LdArgs is
                        // hard...
                        auto phi = new Phi;
                        for (auto a : v.vals) {
                            phi->push_arg(a);
                        }
                        phi->updateType();
                        ld->replaceUsesWith(phi);
                        if (needEnv) {
                            ip++;
                            next = bb->insert(ip, phi);
                        } else {
                            bb->replace(ip, phi);
                        }
                    }
                }
                ip = next;
            }
        });
    }
};
}

namespace rir {
namespace pir {

void ScopeResolution::apply(Function* function) {
    TheScopeResolution s(function);
    s();
}
}
}
