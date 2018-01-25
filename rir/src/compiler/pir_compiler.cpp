#include "pir_compiler.h"
#include "R/RList.h"
#include "analysis/query.h"
#include "analysis/verifier.h"
#include "ir/BC.h"
#include "opt/cleanup.h"
#include "opt/scope_resolution.h"
#include "pir/pir_impl.h"
#include "transform/insert_cast.h"
#include "util/builder.h"

#include <deque>
#include <vector>

namespace {

using namespace rir::pir;
typedef rir::pir::Function Function;
typedef rir::Opcode Opcode;
typedef rir::BC BC;
typedef rir::RList RList;

class TheCompiler;

class CodeCompiler {
  public:
    Builder& b;
    rir::Function* src;
    rir::Code* srcCode;
    TheCompiler* cmp;

    CodeCompiler(Builder& b, rir::Function* src, rir::Code* srcCode,
                 TheCompiler* cmp)
        : b(b), src(src), srcCode(srcCode), cmp(cmp) {}

    struct MachineState {
        std::deque<Value*> stack;
        Opcode* pc;
        BB* entry = nullptr;

        size_t stack_size() { return stack.size(); }

        void clear() {
            stack.clear();
            entry = nullptr;
            pc = nullptr;
        }
    };

    typedef std::pair<BB*, Value*> ReturnSite;
    std::vector<ReturnSite> results;

    void push(Value* v) { m.stack.push_back(v); }
    Value* top() { return m.stack.back(); }
    Value* pop() {
        auto v = m.stack.back();
        m.stack.pop_back();
        return v;
    }
    bool empty() { return m.stack.empty(); }

    MachineState m;
    std::deque<MachineState> worklist;
    std::unordered_map<Opcode*, MachineState> mergepoint;

    template <size_t SIZE>
    struct Matcher {
        const std::array<Opcode, SIZE> seq;

        typedef std::function<void(Opcode*)> MatcherMaybe;

        bool operator()(Opcode* pc, Opcode* end, MatcherMaybe m) const {
            for (size_t i = 0; i < SIZE; ++i) {
                if (*pc != seq[i])
                    return false;
                BC::advance(&pc);
                if (pc == end)
                    return false;
            }
            m(pc);
            return true;
        }
    };

    Value* operator()(bool addReturn);

    void run(BC bc) {
        Value* v;
        switch (bc.bc) {
        case Opcode::push_:
            push(b(new LdConst(bc.immediateConst())));
            break;
        case Opcode::ldvar_:
            push(b(new LdVar(bc.immediateConst(), b.env)));
            break;
        case Opcode::stvar_:
            v = pop();
            b(new StVar(bc.immediateConst(), v, b.env));
            break;
        case Opcode::ret_:
            results.push_back(ReturnSite(b.bb, pop()));
            assert(empty());
            break;
        case Opcode::asbool_:
            push(b(new AsLogical(pop())));
            break;
        case Opcode::ldfun_:
            push(b(new LdFun(bc.immediateConst(), b.env)));
            break;
        case Opcode::guard_fun_:
            std::cout << "warn: guard ignored "
                      << CHAR(PRINTNAME(
                             rir::Pool::get(bc.immediate.guard_fun_args.name)))
                      << "\n";
            break;
        case Opcode::dup_:
            push(top());
            break;
        case Opcode::close_: {
            Value* x = pop();
            Value* y = pop();
            Value* z = pop();
            push(b(new MkCls(x, y, z, b.env)));
            break;
        }
        case Opcode::set_shared_:
            // TODO
            break;
        case Opcode::invisible_:
            // TODO
            break;
        case Opcode::visible_:
            // TODO
            break;
        case Opcode::isfun_:
            break;
        case Opcode::pop_:
            pop();
            break;
        case Opcode::call_: {
            unsigned n = bc.immediate.call_args.nargs;
            rir::CallSite* cs = bc.callSite(srcCode);

            std::vector<Value*> args;
            for (size_t i = 0; i < n; ++i) {
                unsigned argi = cs->args()[i];
                if (argi == DOTS_ARG_IDX) {
                    assert(false);
                } else if (argi == MISSING_ARG_IDX) {
                    assert(false);
                }
                rir::Code* code = src->codeAt(argi);
                Promise* prom = b.function->createProm();
                {
                    Builder pb(b.function, prom, b.function->env, prom->entry);
                    CodeCompiler c(pb, src, code, cmp);
                    c(true);
                }
                Value* val = Missing::instance();
                if (Query::pure(prom)) {
                    CodeCompiler c(b, src, code, cmp);
                    val = c(false);
                }
                args.push_back(b(new MkArg(prom, val, b.env)));
            }

            push(b(new Call(b.env, pop(), args)));
            break;
        }
        default:
            std::cerr << "Cannot compile Function. Unsupported bc\n";
            bc.print();
            assert(false);
            break;
        }
    }

    void recoverCfg() {
        std::unordered_map<Opcode*, std::vector<Opcode*>> incom;
        // Mark incoming jmps
        for (auto pc = srcCode->code(); pc != srcCode->endCode();) {
            BC bc = BC::decode(pc);
            if (bc.isJmp()) {
                incom[bc.jmpTarget(pc)].push_back(pc);
            }
            BC::advance(&pc);
        }
        // Mark falltrough to label
        for (auto pc = srcCode->code(); pc != srcCode->endCode();) {
            BC bc = BC::decode(pc);
            if (!bc.isUncondJmp()) {
                Opcode* next = BC::next(pc);
                if (incom.count(next))
                    incom[next].push_back(pc);
            }
            BC::advance(&pc);
        }
        // Create mergepoints
        for (auto m : incom) {
            if (std::get<1>(m).size() > 1) {
                mergepoint[std::get<0>(m)] = MachineState();
            }
        }
    }

    bool doMerge(Opcode* trg) {
        MachineState& s = mergepoint.at(trg);
        if (s.entry == nullptr) {
            s.entry = b.createBB();
            s.pc = trg;
            for (size_t i = 0; i < m.stack_size(); ++i) {
                auto v = pop();
                auto p = new Phi;
                s.entry->append(p);
                p->push_arg(v);
                s.stack.push_front(p);
            }
            return true;
        }

        assert(m.stack_size() == s.stack_size());
        for (size_t i = 0; i < m.stack_size(); ++i) {
            Phi* p = Phi::Cast(s.stack.at(i));
            assert(p);
            p->push_arg(m.stack.at(i));
        }
        return false;
    }

    void pop_worklist() {
        assert(!worklist.empty());
        m = worklist.back();
        worklist.pop_back();
        b.bb = m.entry;
    }
};

class FunctionCompiler {
  public:
    rir::Function* src;
    Env* enclos;
    std::vector<SEXP> args;
    TheCompiler* cmp;

    FunctionCompiler(rir::Function* src, Env* enclos, std::vector<SEXP> args,
                     TheCompiler* cmp)
        : src(src), enclos(enclos), args(args), cmp(cmp) {}
    Function* operator()();
};

class TheCompiler {
  public:
    Module* m;
    TheCompiler() : m(new Module) {}

    Env* createEnv(Env* parent) {
        auto e = new Env(parent);
        m->env.push_back(e);
        return e;
    }

    Function* declare(rir::Function* src, Env* e, const std::vector<SEXP>& a) {
        Function* fun = new Function(a, e);
        // TODO: catch duplicates
        m->function.push_back(fun);
        return fun;
    }

    void operator()(SEXP in) {
        assert(isValidClosureSEXP(in));
        DispatchTable* tbl = DispatchTable::unpack(BODY(in));
        rir::Function* fun = tbl->first();
        auto formals = RList(FORMALS(in));

        std::vector<SEXP> fml;
        for (auto it = formals.begin(); it != formals.end(); ++it) {
            fml.push_back(it.tag());
        }

        Env* e = createEnv(nullptr);
        FunctionCompiler f(fun, e, fml, this);
        f();
    }
};

Function* FunctionCompiler::operator()() {
    Env* e = cmp->createEnv(enclos);
    Function* f = cmp->declare(src, e, args);
    Builder b(f, f, f->env, f->entry);
    CodeCompiler c(b, src, src->body(), cmp);
    c(true);

    ScopeResolution::apply(f);
    Cleanup::apply(f);

    Verifier v(f);
    v();
    return f;
}

Value* CodeCompiler::operator()(bool addReturn) {
    recoverCfg();

    m.pc = srcCode->code();
    Opcode* end = srcCode->endCode();

    while (m.pc != end || !worklist.empty()) {
        if (m.pc == end)
            pop_worklist();

        BC bc = BC::decode(m.pc);

        if (mergepoint.count(m.pc) > 0) {
            bool todo = doMerge(m.pc);
            m = mergepoint.at(m.pc);
            b.next(m.entry);
            if (!todo) {
                if (worklist.empty()) {
                    m.clear();
                    break;
                }
                pop_worklist();
                continue;
            }
        }

        if (bc.isJmp()) {
            auto trg = bc.jmpTarget(m.pc);
            if (bc.isUncondJmp()) {
                m.pc = trg;
                continue;
            }
            // Conditional jump
            Opcode* fallpc = BC::next(m.pc);
            Value* v = pop();
            b(new Branch(v));

            BB* branch = b.createBB();
            BB* fall = b.createBB();

            switch (bc.bc) {
            case Opcode::brtrue_:
                b.bb->next0 = fall;
                b.bb->next1 = branch;
                break;
            case Opcode::brfalse_:
                b.bb->next0 = branch;
                b.bb->next1 = fall;
            default:
                assert(false);
            }

            m.pc = trg;
            m.entry = branch;
            worklist.push_back(m);
            m.pc = fallpc;
            m.entry = fall;
            b.bb = fall;
            continue;
        }

        const static Matcher<4> ifFunctionLiteral(
            {{{Opcode::push_, Opcode::push_, Opcode::push_, Opcode::close_}}});

        bool matched = false;

        ifFunctionLiteral(m.pc, end, [&](Opcode* next) {
            BC ldfmls = BC::advance(&m.pc);
            BC ldcode = BC::advance(&m.pc);
            /* BC ldsrc = */ BC::advance(&m.pc);
            BC::advance(&m.pc); // close

            auto fmlsl = RList(ldfmls.immediateConst());
            SEXP code = ldcode.immediateConst();
            // SEXP src = ldsrc.immediateConst();

            std::vector<SEXP> fmls;
            for (auto it = fmlsl.begin(); it != fmlsl.end(); ++it) {
                fmls.push_back(it.tag());
            }

            DispatchTable* dt = DispatchTable::unpack(code);
            rir::Function* fun = dt->first();

            FunctionCompiler f(fun, b.env, fmls, cmp);
            Function* innerF = f();

            push(b(new MkClsFun(innerF, b.env)));

            matched = true;
            m.pc = next;
        });

        if (!matched) {
            run(bc);
            BC::advance(&m.pc);
        }
    }
    assert(empty());
    InsertCast c(b.code->entry);
    c();

    Value* res;
    assert(results.size() > 0);
    if (results.size() == 1) {
        b.bb = std::get<0>(results.back());
        res = std::get<1>(results.back());
    } else {
        BB* merge = b.createBB();
        Phi* phi = b(new Phi());
        for (auto r : results) {
            std::get<0>(r)->next0 = merge;
            phi->push_arg(std::get<1>(r));
        }
        phi->updateType();
        res = phi;
    }

    while (!(PirType::val() >= res->type)) {
        res = b(InsertCast::cast(res, PirType::val()));
    }

    results.clear();
    if (addReturn)
        b(new Return(res));
    return res;
}
}

namespace rir {

void PirCompiler::compileFunction(SEXP f) {
    TheCompiler cmp;
    cmp(f);

    cmp.m->print(std::cout);
    delete cmp.m;
}
}
