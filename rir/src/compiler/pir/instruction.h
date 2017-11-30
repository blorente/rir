#ifndef COMPILER_INSTRUCTION_H
#define COMPILER_INSTRUCTION_H

#include "R/r.h"
#include "env.h"
#include "value.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>

#define COMPILER_INSTRUCTIONS(V) COMPILER_REAL_INSTRUCTIONS(V)

#define COMPILER_REAL_INSTRUCTIONS(V)                                          \
    V(LdFun)                                                                   \
    V(LdVar)                                                                   \
    V(LdConst)                                                                 \
    V(StVar)                                                                   \
    V(Branch)                                                                  \
    V(Return)                                                                  \
    V(LdArg)                                                                   \
    V(ChkMissing)                                                              \
    V(Call)                                                                    \
    V(Force)

namespace rir {
namespace pir {

class BB;

enum class ITag : uint8_t {
#define V(I) I,
    COMPILER_INSTRUCTIONS(V)
#undef V
};

class Instruction : public Value {
  public:
    typedef std::pair<unsigned, unsigned> Id;
    const ITag tag;
    BB* bb_;
    BB* bb() override { return bb_; }

    Instruction(ITag tag, RType t) : Value(t), tag(tag) {}
    virtual ~Instruction() {}

    Id id();

    typedef std::function<void(Value*, RType)> arg_iterator;
    virtual void each_arg(arg_iterator it) = 0;

    virtual void printRhs(std::ostream& = std::cout) = 0;
    void print(std::ostream& = std::cout);
    void printRef(std::ostream& out) override;

    virtual Value* arg(unsigned pos, Value* v) = 0;
    virtual Value* arg(unsigned pos) = 0;
};

template <ITag class_tag, class Base, unsigned ARGS>
class AnInstruction : public Instruction {
    std::array<Value*, ARGS> arg_;

  public:
    virtual unsigned arguments() const { return ARGS; }

    AnInstruction(AnInstruction&) = delete;
    void operator=(AnInstruction&) = delete;
    AnInstruction() = delete;

    const std::array<RType, ARGS> arg_type;

    template <unsigned pos>
    Value* arg(Value* v) {
        static_assert(pos < ARGS, "This instruction has less arguments");
        arg_[pos] = v;
        return v;
    }

    template <unsigned pos>
    Value* arg() {
        static_assert(pos < ARGS, "This instruction has less arguments");
        return arg_[pos];
    }

    Value* arg(unsigned pos, Value* v) override {
        assert(pos < ARGS && "This instruction has less arguments");
        arg_[pos] = v;
        return v;
    }

    Value* arg(unsigned pos) override {
        assert(pos < ARGS && "This instruction has less arguments");
        return arg_[pos];
    }

    void each_arg(arg_iterator it) override {
        for (unsigned i = 0; i < ARGS; ++i) {
            Value* v = arg(i);
            RType t = arg_type[i];
            it(v, t);
        }
    }

    AnInstruction(RType return_type, const std::array<RType, ARGS>& at,
                  const std::array<Value*, ARGS>& arg)
        : Instruction(class_tag, return_type), arg_type(at), arg_(arg) {}

    AnInstruction(RType return_type, const std::array<RType, ARGS>& at)
        : Instruction(class_tag, return_type), arg_type(at) {}

    static Base* cast(Instruction* i) {
        if (i->tag == class_tag)
            return static_cast<Base*>(i);
        return nullptr;
    }
};

extern std::ostream& operator<<(std::ostream& out, Instruction::Id id);

class LdConst : public AnInstruction<ITag::LdConst, LdConst, 0> {
  public:
    LdConst(SEXP c, RType t) : AnInstruction(t, {}), c(c) {}
    LdConst(SEXP c) : AnInstruction(RType::value, {}), c(c) {}
    SEXP c;
    void printRhs(std::ostream& out) override;
};

class LdFun : public AnInstruction<ITag::LdFun, LdFun, 0> {
  public:
    SEXP name;
    Env* env;

    LdFun(const char* name, Env* env)
        : AnInstruction(RType::closure, {}), name(Rf_install(name)), env(env) {}
    LdFun(SEXP name, Env* env)
        : AnInstruction(RType::closure, {}), name(name), env(env) {
        assert(TYPEOF(name) == SYMSXP);
    }

    void printRhs(std::ostream& out) override {
        out << "ldfun " << CHAR(PRINTNAME(name)) << ", " << *env;
    }
};

class LdVar : public AnInstruction<ITag::LdVar, LdVar, 0> {
  public:
    SEXP name;
    Env* env;

    LdVar(const char* name, Env* env)
        : AnInstruction(RType::maybeVal, {}), name(Rf_install(name)), env(env) {
    }
    LdVar(SEXP name, Env* env)
        : AnInstruction(RType::maybeVal, {}), name(name), env(env) {
        assert(TYPEOF(name) == SYMSXP);
    }

    void printRhs(std::ostream& out) override {
        out << "ldvar " << CHAR(PRINTNAME(name)) << ", " << *env;
    }
};

class ChkMissing : public AnInstruction<ITag::ChkMissing, ChkMissing, 1> {
  public:
    ChkMissing(Value* in) : AnInstruction(RType::value, {{RType::maybeVal}}) {
        arg<0>(in);
    }

    void printRhs(std::ostream& out) override {
        out << "chk_missing ";
        arg<0>()->printRef(out);
    }
};

class StVar : public AnInstruction<ITag::StVar, StVar, 1> {
  public:
    StVar(SEXP name, Value* val, Env* env)
        : AnInstruction(RType::voyd, {{RType::value}}), name(name), env(env) {
        arg<0>(val);
    }
    StVar(const char* name, Value* val, Env* env)
        : AnInstruction(RType::voyd, {{RType::value}}), name(Rf_install(name)),
          env(env) {
        arg<0>(val);
    }

    SEXP name;
    Value* val() { return arg<0>(); }
    Env* env;

    void printRhs(std::ostream& out) override {
        out << "stvar " << CHAR(PRINTNAME(name)) << ", ";
        val()->printRef(out);
        out << ", " << *env;
    }
};

class Branch : public AnInstruction<ITag::Branch, Branch, 1> {
  public:
    Branch(Value* test) : AnInstruction(RType::voyd, {{RType::test}}) {
        arg<0>(test);
    }
    void printRhs(std::ostream& out) override;
};

class Return : public AnInstruction<ITag::Return, Return, 1> {
  public:
    Return(Value* ret) : AnInstruction(RType::voyd, {{RType::value}}) {
        arg<0>(ret);
    }
    void printRhs(std::ostream& out) override {
        out << "return ";
        arg<0>()->printRef(out);
    }
};

class Promise;
class LdArg : public AnInstruction<ITag::LdArg, LdArg, 0> {
  public:
    Promise* prom;
    Env* env;
    LdArg(Promise* prom, Env* env)
        : AnInstruction(RType::prom, {}), prom(prom), env(env) {}
    void printRhs(std::ostream& out) override;
};

template <RType T, RType... Ts>
struct seq {};
template <unsigned N, RType T, RType... Ts>
struct gen_seq : gen_seq<N - 1, T, Ts...> {};
template <RType T, RType... Ts>
struct gen_seq<0, T, Ts...> : seq<T, T, Ts...> {};

template <unsigned ARGS>
class Call : public AnInstruction<ITag::Call, Call<ARGS>, ARGS + 1> {
    template <RType... Ts>
    static constexpr std::array<RType, ARGS + 1> gen_signature(seq<Ts...>) {
        return {RType::closure, Ts...};
    };
    typedef AnInstruction<ITag::Call, Call<ARGS>, ARGS + 1> Super;

  public:
    Call(Env* e, Value* fun, const std::array<Value*, ARGS>& args)
        : Super::AnInstruction(RType::any,
                               gen_signature(gen_seq<ARGS, RType::arg>{})) {
        this->arg(0, fun);
        for (unsigned i = 1; i <= ARGS; ++i)
            this->arg(i, args[i - 1]);
    }

    void printRhs(std::ostream& out) override {
        out << "call ";
        this->arg(0)->printRef(out);
        out << " (";
        for (unsigned i = 1; i < ARGS; ++i) {
            this->arg(i)->printRef(out);
            out << ", ";
        }
        this->arg(ARGS)->printRef(out);
        out << ")";
    }
};

class Force : public AnInstruction<ITag::Force, Force, 1> {
  public:
    Force(Value* in) : AnInstruction(RType::value, {{RType::any}}) {
        arg<0>(in);
    }

    void printRhs(std::ostream& out) override {
        out << "force ";
        arg<0>()->printRef(out);
    }
};
}
}

#endif
