#ifndef COMPILER_INSTRUCTION_H
#define COMPILER_INSTRUCTION_H

#include "R/r.h"
#include "pir.h"
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
    V(Phi)                                                                     \
    V(AsLogical)                                                               \
    V(AsTest)                                                                  \
    V(Return)                                                                  \
    V(MkArg)                                                                   \
    V(MkCls)                                                                   \
    V(MkClsFun)                                                                \
    V(ChkMissing)                                                              \
    V(ChkClosure)                                                              \
    V(Call)                                                                    \
    V(Force)

namespace rir {
namespace pir {

class BB;
class Function;

enum class ITag : uint8_t {
#define V(I) I,
    COMPILER_INSTRUCTIONS(V)
#undef V
};

static const char* InstructionName(ITag tag) {
    switch (tag) {
#define V(I)                                                                   \
    case ITag::I:                                                              \
        return #I;
        COMPILER_INSTRUCTIONS(V)
#undef V
    }
    assert(false);
    return "";
}

class Instruction : public Value {
  protected:
    virtual size_t nargs() = 0;
    virtual Value** args() = 0;
    virtual const PirType* types() = 0;

  public:
    virtual bool pure() = 0;

    typedef std::pair<unsigned, unsigned> Id;
    const ITag tag;
    BB* bb_;
    BB* bb() {
        assert(bb_);
        return bb_;
    }
    void bb(bbMaybe maybe) override { return maybe(bb()); }

    Instruction(ITag tag, PirType t) : Value(t, Kind::instruction), tag(tag) {}
    virtual ~Instruction() {}

    Id id();

    typedef std::function<void(Value*, PirType)> arg_iterator;
    typedef std::function<Value*(Value*, PirType)> arg_map_iterator;

    const char* name() { return InstructionName(tag); }

    virtual void printRhs(std::ostream& out = std::cout) {
        out << name() << " (";
        if (nargs() > 0) {
            for (size_t i = 0; i < nargs() - 1; ++i) {
                arg(i)->printRef(out);
                out << ", ";
            }
            arg(nargs() - 1)->printRef(out);
        }
        out << ")";
    }

    void print(std::ostream& = std::cout);
    void printRef(std::ostream& out) override;

    Value* arg(size_t pos, Value* v) {
        assert(pos < nargs() && "This instruction has less arguments");
        args()[pos] = v;
        return v;
    }

    Value* arg(size_t pos) {
        assert(pos < nargs() && "This instruction has less arguments");
        return args()[pos];
    }

    void each_arg(arg_iterator it) {
        for (size_t i = 0; i < nargs(); ++i) {
            Value* v = arg(i);
            PirType t = types()[i];
            it(v, t);
        }
    }

    void map_arg(arg_map_iterator it) {
        for (size_t i = 0; i < nargs(); ++i) {
            Value* v = arg(i);
            PirType t = types()[i];
            arg(i, it(v, t));
        }
    }
};

class InstructionWithEnv : public Instruction {
    Env* env_;

  public:
    Env* env() { return env_; }
    InstructionWithEnv(ITag tag, PirType t, Env* env)
        : Instruction(tag, t), env_(env) {}
};

template <ITag class_tag, class Base, size_t ARGS, bool PURE,
          class Super = Instruction>
class AnInstruction : public Super {
    std::array<Value*, ARGS> arg_;

  protected:
    size_t nargs() override { return ARGS; }
    Value** args() override { return &arg_[0]; }
    const PirType* types() override { return &arg_type[0]; }

  public:
    bool pure() override { return PURE; }

    AnInstruction(AnInstruction&) = delete;
    void operator=(AnInstruction&) = delete;
    AnInstruction() = delete;

    const std::array<PirType, ARGS> arg_type;

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

    template <unsigned pos>
    PirType type() {
        static_assert(pos < ARGS, "This instruction has less arguments");
        return arg_type[pos];
    }

    AnInstruction(PirType return_type, const std::array<PirType, ARGS>& at,
                  const std::array<Value*, ARGS>& arg)
        : Super(class_tag, return_type), arg_(arg), arg_type(at) {}

    AnInstruction(PirType return_type)
        : Super(class_tag, return_type), arg_type({}) {
        static_assert(ARGS == 0, "Missing arguments");
    }

    AnInstruction(PirType return_type, const std::array<PirType, ARGS>& at,
                  const std::array<Value*, ARGS>& arg, Env* env)
        : Super(class_tag, return_type, env), arg_(arg), arg_type(at) {}

    AnInstruction(PirType return_type, Env* env)
        : Super(class_tag, return_type, env), arg_type({}) {
        static_assert(ARGS == 0, "Missing arguments");
    }

    static Base* cast(Value* v) {
        if (v->kind == Kind::instruction)
            return cast(static_cast<Instruction*>(v));
        return nullptr;
    }

    static Base* cast(Instruction* i) {
        if (i->tag == class_tag)
            return static_cast<Base*>(i);
        return nullptr;
    }
};

template <ITag class_tag, class Base, bool PURE, class Super = Instruction>
class VarArgInstruction : public Super {
    std::vector<Value*> arg_;
    std::vector<PirType> arg_type;

  protected:
    size_t nargs() override { return arg_.size(); }
    Value** args() override { return arg_.data(); }
    const PirType* types() override { return arg_type.data(); }

  public:
    bool pure() override { return PURE; }

    VarArgInstruction(VarArgInstruction&) = delete;
    void operator=(VarArgInstruction&) = delete;
    VarArgInstruction() = delete;

    void push_arg(Value* a) {
        assert(arg_.size() == arg_type.size());
        arg_type.push_back(a->type);
        arg_.push_back(a);
    }

    void push_arg(PirType t, Value* a) {
        assert(arg_.size() == arg_type.size());
        assert(t >= a->type);
        arg_type.push_back(t);
        arg_.push_back(a);
    }

    VarArgInstruction(PirType return_type) : Super(class_tag, return_type) {}

    VarArgInstruction(PirType return_type, Env* env)
        : Super(class_tag, return_type, env) {}

    static Base* cast(Value* v) {
        if (v->kind == Kind::instruction)
            return cast(static_cast<Instruction*>(v));
        return nullptr;
    }

    static Base* cast(Instruction* i) {
        if (i->tag == class_tag)
            return static_cast<Base*>(i);
        return nullptr;
    }
};

extern std::ostream& operator<<(std::ostream& out, Instruction::Id id);

class LdConst : public AnInstruction<ITag::LdConst, LdConst, 0, true> {
  public:
    LdConst(SEXP c, PirType t) : AnInstruction(t), c(c) {}
    LdConst(SEXP c) : AnInstruction(PirType::val()), c(c) {}
    SEXP c;
    void printRhs(std::ostream& out) override;
};

class LdFun
    : public AnInstruction<ITag::LdFun, LdFun, 0, false, InstructionWithEnv> {
  public:
    SEXP varName;

    LdFun(const char* name, Env* env)
        : AnInstruction(RType::closure, env), varName(Rf_install(name)) {}
    LdFun(SEXP name, Env* env)
        : AnInstruction(RType::closure, env), varName(name) {
        assert(TYPEOF(name) == SYMSXP);
    }
};

class LdVar
    : public AnInstruction<ITag::LdVar, LdVar, 0, true, InstructionWithEnv> {
  public:
    SEXP varName;

    LdVar(const char* name, Env* env)
        : AnInstruction(PirType::any(), env), varName(Rf_install(name)) {}
    LdVar(SEXP name, Env* env)
        : AnInstruction(PirType::any(), env), varName(name) {
        assert(TYPEOF(name) == SYMSXP);
    }

    void printRhs(std::ostream& out) override;
};

class ChkMissing
    : public AnInstruction<ITag::ChkMissing, ChkMissing, 1, false> {
  public:
    ChkMissing(Value* in)
        : AnInstruction(PirType::val(), {{PirType::valOrMissing()}}, {{in}}) {}
};

class ChkClosure
    : public AnInstruction<ITag::ChkClosure, ChkClosure, 1, false> {
  public:
    ChkClosure(Value* in)
        : AnInstruction(RType::closure, {{PirType::val()}}, {{in}}) {}
};

class StVar
    : public AnInstruction<ITag::StVar, StVar, 1, false, InstructionWithEnv> {
  public:
    StVar(SEXP name, Value* val, Env* env)
        : AnInstruction(PirType::voyd(), {{PirType::val()}}, {{val}}, env),
          varName(name) {}

    StVar(const char* name, Value* val, Env* env)
        : AnInstruction(PirType::voyd(), {{PirType::val()}}, {{val}}, env),
          varName(Rf_install(name)) {}

    SEXP varName;
    Value* val() { return arg<0>(); }

    void printRhs(std::ostream& out) override;
};

class Branch : public AnInstruction<ITag::Branch, Branch, 1, true> {
  public:
    Branch(Value* test)
        : AnInstruction(PirType::voyd(), {{NativeType::test}}, {{test}}) {}
    void printRhs(std::ostream& out) override;
};

class Return : public AnInstruction<ITag::Return, Return, 1, true> {
  public:
    Return(Value* ret)
        : AnInstruction(PirType::voyd(), {{PirType::val()}}, {{ret}}) {}
};

class Promise;
class MkArg
    : public AnInstruction<ITag::MkArg, MkArg, 1, true, InstructionWithEnv> {
  public:
    Promise* prom;
    MkArg(Promise* prom, Value* v, Env* env)
        : AnInstruction(RType::prom, {{PirType::valOrMissing()}}, {{v}}, env),
          prom(prom) {}
    void printRhs(std::ostream& out) override;
};

class MkCls
    : public AnInstruction<ITag::MkCls, MkCls, 3, true, InstructionWithEnv> {
  public:
    MkCls(Value* code, Value* arg, Value* src, Env* parent)
        : AnInstruction(RType::closure,
                        {{RType::code, PirType::list(), PirType::any()}},
                        {{code, arg, src}}, parent) {}
};

class MkClsFun : public AnInstruction<ITag::MkClsFun, MkClsFun, 0, true,
                                      InstructionWithEnv> {
  public:
    Function* fun;
    MkClsFun(Function* fun, Env* env)
        : AnInstruction(RType::closure, env), fun(fun) {}
    void printRhs(std::ostream& out) override;
};

class Call
    : public VarArgInstruction<ITag::Call, Call, false, InstructionWithEnv> {
  public:
    Call(Env* e, Value* fun, const std::vector<Value*>& args)
        : VarArgInstruction(PirType::any(), e) {
        this->push_arg(RType::closure, fun);
        for (unsigned i = 1; i <= args.size(); ++i)
            this->push_arg(RType::prom, args[i - 1]);
    }

    void printRhs(std::ostream& out) override;
};

class Force : public AnInstruction<ITag::Force, Force, 1, false> {
  public:
    Force(Value* in)
        : AnInstruction(PirType::valOrMissing(), {{PirType::any()}}, {{in}}) {}
};

class AsLogical : public AnInstruction<ITag::AsLogical, AsLogical, 1, false> {
  public:
    AsLogical(Value* in)
        : AnInstruction(RType::logical, {{PirType::val()}}, {{in}}) {}
};

class AsTest : public AnInstruction<ITag::AsTest, AsTest, 1, true> {
  public:
    AsTest(Value* in)
        : AnInstruction(NativeType::test, {{RType::logical}}, {{in}}) {}
};

class Phi : public VarArgInstruction<ITag::Phi, Phi, true> {
  public:
    Phi() : VarArgInstruction(PirType::any()) {}
};
}
}

#endif
