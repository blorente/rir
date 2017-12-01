#include "pir_compiler.h"
#include "R/RList.h"
#include "ir/BC.h"
#include "pir/builder.h"
#include "pir/env.h"
#include "pir/function.h"
#include "pir/instruction.h"
#include "pir/verifier.h"

#include <deque>
#include <vector>

namespace {

using namespace rir;

class FunctionCompiler {
    typedef pir::RType RType;
    typedef pir::BB BB;
    typedef pir::Value Value;

  public:
    struct MachineState {
        std::unordered_map<SEXP, pir::Value*> env_analysis;
        std::deque<pir::Value*> stack;
        Opcode* pc;
        BB* entry = nullptr;

        size_t stack_size() { return stack.size(); }
    };

    void push(pir::Value* v) { m.stack.push_back(v); }
    pir::Value* pop() {
        auto v = m.stack.back();
        m.stack.pop_back();
        return v;
    }
    pir::Value* pop(RType t) {
        pir::Value* v = pop();
        if (subtype(v->type, t))
            return v;
        return cast(v, t);
    }
    bool empty() { return m.stack.empty(); }

    MachineState m;
    std::deque<MachineState> worklist;
    std::unordered_map<Opcode*, MachineState> mergepoint;

    Function* src;

    pir::Env* e;
    pir::Function* f;
    pir::Builder b;
    Opcode* end;

    FunctionCompiler(Function* src, pir::Env* e, const std::vector<SEXP>& a)
        : src(src), e(e), f(new pir::Function(a, e)), b(f) {
        m.pc = src->body()->code();
        end = src->body()->endCode();
    }

    pir::Value* cast(pir::Value* v, RType t) {
        if (subtype(v->type, t))
            return v;
        if (t == RType::value) {
            if (v->type == RType::maybeVal) {
                return b(new pir::ChkMissing(v));
            }
            if (v->type == RType::any) {
                return b(new pir::ChkMissing(b(new pir::Force(v))));
            }
        }
        if (t == RType::maybeVal) {
            if (v->type == RType::any) {
                return b(new pir::Force(v));
            }
        }
        if (v->type == RType::logical && t == RType::test) {
            return b(new pir::AsTest(v));
        }
        std::cerr << "Cannot cast " << v->type << " to " << t << "\n";
        assert(false);
        return v;
    }

    void run(BC bc) {
        pir::Value* v;
        switch (bc.bc) {
        case Opcode::push_:
            push(b(new pir::LdConst(bc.immediateConst())));
            break;
        case Opcode::ldvar_:
            if (m.env_analysis.count(bc.immediateConst()))
                push(m.env_analysis[bc.immediateConst()]);
            else
                push(b(new pir::LdVar(bc.immediateConst(), f->env)));
            break;
        case Opcode::stvar_:
            v = pop(RType::value);
            m.env_analysis[bc.immediateConst()] = v;
            b(new pir::StVar(bc.immediateConst(), v, f->env));
            break;
        case Opcode::ret_:
            b(new pir::Return(pop(RType::value)));
            break;
        case Opcode::asbool_:
            push(b(new pir::AsLogical(pop(RType::value))));
            break;
        case Opcode::guard_fun_:
            std::cout << "warn: guard ignored "
                      << CHAR(PRINTNAME(
                             Pool::get(bc.immediate.guard_fun_args.name)))
                      << "\n";
            break;
        default:
            std::cerr << "Cannot compile Function. Unsupported bc\n";
            bc.print();
            assert(false);
            break;
        }
    }

    void recoverCfg(Code* code) {
        std::unordered_map<Opcode*, std::vector<Opcode*>> incom;
        // Mark incoming jmps
        for (auto pc = code->code(); pc != code->endCode();) {
            BC bc = BC::decode(pc);
            if (bc.isJmp()) {
                incom[bc.jmpTarget(pc)].push_back(pc);
            }
            BC::advance(&pc);
        }
        // Mark falltrough to label
        for (auto pc = code->code(); pc != code->endCode();) {
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
                auto p = new pir::Phi;
                *s.entry << p;
                p->push_arg(v);
                s.stack.push_front(p);
            }
            return true;
        }

        assert(m.stack_size() == s.stack_size());
        for (size_t i = 0; i < m.stack_size(); ++i) {
            pir::Phi* p = pir::Phi::cast(s.stack.at(i));
            assert(p);
            p->push_arg(m.stack.at(i));
        }
        return false;
    }

    void pop_worklist() {
        if (worklist.empty()) {
            m.pc = end;
            m.stack.clear();
            return;
        }
        m = worklist.back();
        worklist.pop_back();
        b.bb = m.entry;
    }

    void operator()() {
        recoverCfg(src->body());

        while (m.pc != end || !worklist.empty()) {
            if (m.pc == end)
                pop_worklist();

            BC bc = BC::decode(m.pc);

            if (mergepoint.count(m.pc) > 0) {
                bool todo = doMerge(m.pc);
                m = mergepoint.at(m.pc);
                b.next(m.entry);
                if (!todo) {
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
                Value* v = pop(RType::test);
                b(new pir::Branch(v));

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
            run(bc);
            BC::advance(&m.pc);
        }
        assert(empty());
        pir::Verifier v(f);
        v();
    }
};

class TheCompiler {
  public:
    pir::Module* m;
    TheCompiler() : m(new pir::Module) {}

    void operator()(Function* src, const std::vector<SEXP>& a) {
        pir::Env* e = new pir::Env;
        FunctionCompiler f(src, e, a);
        f();
        m->env.push_back(e);
        m->function.push_back(f.f);
    }
};
}

namespace rir {

void PirCompiler::compileFunction(Function* fun, RList formals) {
    std::vector<SEXP> fml;
    for (auto f : formals)
        fml.push_back(f);

    TheCompiler cmp;
    cmp(fun, fml);

    cmp.m->print(std::cout);
    delete cmp.m;
}
}
