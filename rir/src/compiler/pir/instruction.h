#ifndef COMPILER_INSTRUCTION_H
#define COMPILER_INSTRUCTION_H

#include "R/r.h"
#include "instruction_list.h"
#include "pir.h"
#include "value.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>

namespace rir {
namespace pir {

class BB;
class Function;

class Instruction : public Value {
  protected:
    virtual Value** args() = 0;
    virtual const PirType* types() = 0;

  public:
    virtual size_t nargs() = 0;

    virtual bool mightIO() = 0;
    virtual bool changesEnv() = 0;
    virtual bool leaksEnv() { return false; }
    virtual bool needsEnv() { return false; }
    virtual Value* env() = 0;

    virtual Instruction* clone() = 0;

    typedef std::pair<unsigned, unsigned> Id;
    const Tag tag;
    BB* bb_;
    BB* bb() {
        assert(bb_);
        return bb_;
    }
    void bb(bbMaybe maybe) override { return maybe(bb()); }

    Instruction(Tag tag, PirType t) : Value(t, tag), tag(tag) {}
    virtual ~Instruction() {}

    Id id();

    typedef std::function<void(Value*, PirType)> arg_iterator;
    typedef std::function<Value*(Value*, PirType)> arg_map_iterator;

    const char* name() { return TagToStr(tag); }

    void replaceUsesWith(Value* val);
    bool unused();

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

enum class EnvAccess : uint8_t {
    None,
    Read,
    Write,
    Leak,
};

enum class Effect : uint8_t {
    None,
    Warn,
    Error,
    Print,
    Any,
};

template <Tag class_tag, class Base, Effect EFFECT, EnvAccess ENV>
class InstructionDescription : public Instruction {
  public:
    InstructionDescription(PirType return_type)
        : Instruction(class_tag, return_type) {}

    void operator=(InstructionDescription&) = delete;
    InstructionDescription() = delete;

    bool mightIO() override { return EFFECT > Effect::None; }
    bool changesEnv() override { return ENV >= EnvAccess::Write; }
    bool leaksEnv() override { return ENV == EnvAccess::Leak; }
    bool needsEnv() override { return ENV > EnvAccess::None; }

    Instruction* clone() override {
        assert(Base::Cast(this));
        return new Base(*static_cast<Base*>(this));
    }

    static Base* Cast(Value* i) {
        if (i->tag == class_tag)
            return static_cast<Base*>(i);
        return nullptr;
    }

    static void If(Instruction* i, std::function<void()> maybe) {
        Base* b = Cast(i);
        if (b)
            maybe();
    }

    static void If(Instruction* i, std::function<void(Base* b)> maybe) {
        Base* b = Cast(i);
        if (b)
            maybe(b);
    }
};

template <Tag class_tag, class Base, size_t ARGS, Effect EFFECT, EnvAccess ENV>
class FixedLenInstruction
    : public InstructionDescription<class_tag, Base, EFFECT, ENV> {
  private:
    typedef InstructionDescription<class_tag, Base, EFFECT, ENV> Super;
    std::array<Value*, ARGS> arg_;
    const std::array<PirType, ARGS> arg_type;

  protected:
    Value** args() override { return &arg_[0]; }
    const PirType* types() override { return &arg_type[0]; }

  public:
    size_t nargs() override { return ARGS; }

    Value* env() override {
        // TODO find a better way
        assert(ENV != EnvAccess::None);
        Value* env = arg_[ARGS - 1];
        return env;
    }

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

    struct ArgTypesWithEnv : public std::array<PirType, ARGS> {
        ArgTypesWithEnv(const std::array<PirType, ARGS - 1>& a) {
            for (size_t i = 0; i < ARGS - 1; ++i)
                (*this)[i] = a[i];
            (*this)[ARGS - 1] = RType::env;
        }
        ArgTypesWithEnv() : std::array<PirType, ARGS>({{RType::env}}) {}
    };
    struct ArgsWithEnv : public std::array<Value*, ARGS> {
        ArgsWithEnv(const std::array<Value*, ARGS - 1>& a, Value* env) {
            for (size_t i = 0; i < ARGS - 1; ++i)
                (*this)[i] = a[i];
            (*this)[ARGS - 1] = env;
        }
        ArgsWithEnv(Value* env) : std::array<Value*, ARGS>({{env}}) {}
    };

    FixedLenInstruction(PirType return_type, Value* env)
        : Super(return_type), arg_(ArgsWithEnv(env)),
          arg_type(ArgTypesWithEnv()) {
        static_assert(ARGS == 1, "Missing args");
        static_assert(ENV != EnvAccess::None,
                      "This instruction has no environment access");
    }

    FixedLenInstruction(PirType return_type)
        : Super(return_type), arg_({}), arg_type({}) {
        static_assert(ARGS == 0, "Missing args");
        static_assert(ENV == EnvAccess::None,
                      "This instruction needs an environment");
    }

    FixedLenInstruction(PirType return_type,
                        const std::array<PirType, ARGS - 1>& at,
                        const std::array<Value*, ARGS - 1>& arg, Value* env)
        : Super(return_type), arg_(ArgsWithEnv(arg, env)),
          arg_type(ArgTypesWithEnv(at)) {
        static_assert(ENV != EnvAccess::None,
                      "This instruction has no environment access");
        static_assert(ARGS > 1, "Instruction with env but no args?");
    }

    FixedLenInstruction(PirType return_type,
                        const std::array<PirType, ARGS>& at,
                        const std::array<Value*, ARGS>& arg)
        : Super(return_type), arg_(arg), arg_type(at) {
        static_assert(ENV == EnvAccess::None,
                      "This instruction needs an environment");
    }
};

template <Tag class_tag, class Base, Effect EFFECT, EnvAccess ENV>
class VarLenInstruction
    : public InstructionDescription<class_tag, Base, EFFECT, ENV> {
  private:
    typedef InstructionDescription<class_tag, Base, EFFECT, ENV> Super;

    std::vector<Value*> arg_;
    std::vector<PirType> arg_type;
  protected:
    Value** args() override { return arg_.data(); }
    const PirType* types() override { return arg_type.data(); }

  public:
    size_t nargs() override { return arg_.size(); }

    Value* env() override {
        // TODO find a better way
        assert(ENV != EnvAccess::None);
        Value* env = arg_[0];
        return env;
    }

    void push_arg(Value* a) {
        assert(arg_.size() == arg_type.size());
        arg_type.push_back(a->type);
        arg_.push_back(a);
        assert(arg_.size() > 1 || ENV == EnvAccess::None ||
               arg_type[0] == RType::env);
    }

    void push_arg(PirType t, Value* a) {
        assert(arg_.size() == arg_type.size());
        arg_type.push_back(t);
        arg_.push_back(a);
        assert(arg_.size() > 1 || ENV == EnvAccess::None ||
               arg_type[0] == RType::env);
    }

    VarLenInstruction(PirType return_type) : Super(return_type) {
        static_assert(ENV == EnvAccess::None,
                      "This instruction needs an environment");
    }

    VarLenInstruction(PirType return_type, Value* env) : Super(return_type) {
        static_assert(ENV > EnvAccess::None,
                      "This instruction has no environment access");
        push_arg(RType::env, env);
    }
};

extern std::ostream& operator<<(std::ostream& out, Instruction::Id id);

#define FLI(type, nargs, io, env)                                              \
    type:                                                                      \
  public                                                                       \
    FixedLenInstruction<Tag::type, type, nargs, io, env>

class FLI(LdConst, 0, Effect::None, EnvAccess::None) {
  public:
    LdConst(SEXP c, PirType t) : FixedLenInstruction(t), c(c) {}
    LdConst(SEXP c) : FixedLenInstruction(PirType::val()), c(c) {}
    SEXP c;
    void printRhs(std::ostream& out) override;
};

class FLI(LdFun, 1, Effect::Any, EnvAccess::Write) {
  public:
    SEXP varName;

    LdFun(const char* name, Value* env)
        : FixedLenInstruction(RType::closure, env), varName(Rf_install(name)) {}
    LdFun(SEXP name, Value* env)
        : FixedLenInstruction(RType::closure, env), varName(name) {
        assert(TYPEOF(name) == SYMSXP);
    }

    void printRhs(std::ostream& out) override;
};

class FLI(LdVar, 1, Effect::None, EnvAccess::Read) {
  public:
    SEXP varName;

    LdVar(const char* name, Value* env)
        : FixedLenInstruction(PirType::any(), env), varName(Rf_install(name)) {}
    LdVar(SEXP name, Value* env)
        : FixedLenInstruction(PirType::any(), env), varName(name) {
        assert(TYPEOF(name) == SYMSXP);
    }

    void printRhs(std::ostream& out) override;
};

class FLI(LdArg, 1, Effect::None, EnvAccess::Read) {
  public:
    size_t id;

    LdArg(size_t id, Value* env)
        : FixedLenInstruction(PirType::valOrLazy(), env), id(id) {}

    void printRhs(std::ostream& out) override;
};

class FLI(ChkMissing, 1, Effect::Warn, EnvAccess::None) {
  public:
    ChkMissing(Value* in)
        : FixedLenInstruction(PirType::valOrLazy(), {{PirType::any()}},
                              {{in}}) {}
};

class FLI(ChkClosure, 1, Effect::Warn, EnvAccess::None) {
  public:
    ChkClosure(Value* in)
        : FixedLenInstruction(RType::closure, {{PirType::val()}}, {{in}}) {}
};

class FLI(StVar, 2, Effect::None, EnvAccess::Write) {
  public:
    StVar(SEXP name, Value* val, Value* env)
        : FixedLenInstruction(PirType::voyd(), {{PirType::val()}}, {{val}},
                              env),
          varName(name) {}

    StVar(const char* name, Value* val, Value* env)
        : FixedLenInstruction(PirType::voyd(), {{PirType::val()}}, {{val}},
                              env),
          varName(Rf_install(name)) {}

    SEXP varName;
    Value* val() { return arg<0>(); }

    void printRhs(std::ostream& out) override;
};

class FLI(Branch, 1, Effect::None, EnvAccess::None) {
  public:
    Branch(Value* test)
        : FixedLenInstruction(PirType::voyd(), {{NativeType::test}}, {{test}}) {
    }
    void printRhs(std::ostream& out) override;
};

class FLI(Return, 1, Effect::None, EnvAccess::None) {
  public:
    Return(Value* ret)
        : FixedLenInstruction(PirType::voyd(), {{PirType::val()}}, {{ret}}) {}
};

class Promise;
class FLI(MkArg, 2, Effect::None, EnvAccess::Read) {
  public:
    Promise* prom;
    MkArg(Promise* prom, Value* v, Value* env)
        : FixedLenInstruction(RType::prom, {{PirType::valOrMissing()}}, {{v}},
                              env),
          prom(prom) {}
    void printRhs(std::ostream& out) override;
};

class FLI(MkCls, 4, Effect::None, EnvAccess::Read) {
  public:
    MkCls(Value* code, Value* arg, Value* src, Value* parent)
        : FixedLenInstruction(RType::closure,
                              {{RType::code, PirType::list(), PirType::any()}},
                              {{code, arg, src}}, parent) {}
};

class FLI(MkClsFun, 1, Effect::None, EnvAccess::Read) {
  public:
    Function* fun;
    MkClsFun(Function* fun, Value* env)
        : FixedLenInstruction(RType::closure, env), fun(fun) {}
    void printRhs(std::ostream& out) override;
};

class FLI(Force, 1, Effect::Any, EnvAccess::None) {
  public:
    Force(Value* in)
        : FixedLenInstruction(PirType::val(), {{PirType::any()}}, {{in}}) {}
};

class FLI(AsLogical, 1, Effect::Warn, EnvAccess::None) {
  public:
    AsLogical(Value* in)
        : FixedLenInstruction(RType::logical, {{PirType::val()}}, {{in}}) {}
};

class FLI(AsTest, 1, Effect::None, EnvAccess::None) {
  public:
    AsTest(Value* in)
        : FixedLenInstruction(NativeType::test, {{RType::logical}}, {{in}}) {}
};

class FLI(IndexWrite, 3, Effect::None, EnvAccess::None) {
  public:
    IndexWrite(Value* vec, Value* index, Value* value)
        : FixedLenInstruction(
              PirType::val(),
              {{PirType::val(), PirType::val(), PirType::val()}},
              {{vec, index, value}}) {}
};

#define SAFE_BINOP(Name, Type)                                                 \
    class FLI(Name, 2, Effect::None, EnvAccess::None) {                        \
      public:                                                                  \
        Name(Value* a, Value* b)                                               \
            : FixedLenInstruction(Type, {{PirType::val(), PirType::val()}},    \
                                  {{a, b}}) {}                                 \
    }

SAFE_BINOP(Gte, PirType::val());
SAFE_BINOP(Lte, PirType::val());
SAFE_BINOP(Mul, PirType::val());
SAFE_BINOP(Div, PirType::val());
SAFE_BINOP(Mod, PirType::val());
SAFE_BINOP(Add, PirType::val());
SAFE_BINOP(Colon, PirType::val());
SAFE_BINOP(Pow, PirType::val());
SAFE_BINOP(Sub, PirType::val());
SAFE_BINOP(IndexAccess, PirType::val());
SAFE_BINOP(Gt, RType::logical);
SAFE_BINOP(Lt, RType::logical);
SAFE_BINOP(Neq, RType::logical);
SAFE_BINOP(Eq, RType::logical);
SAFE_BINOP(LAnd, RType::logical);
SAFE_BINOP(LOr, RType::logical);

#undef SAFE_BINOP

#define SAFE_UNOP(Name)                                                        \
    class FLI(Name, 1, Effect::None, EnvAccess::None) {                        \
      public:                                                                  \
        Name(Value* v)                                                         \
            : FixedLenInstruction(PirType::val(), {{PirType::val()}}, {{v}}) { \
        }                                                                      \
    }

SAFE_UNOP(Inc);
SAFE_UNOP(Not);
SAFE_UNOP(Length);

#undef SAFE_UNOP
#undef FLI

#define VLI(type, io, env)                                                     \
    type:                                                                      \
  public                                                                       \
    VarLenInstruction<Tag::type, type, io, env>

class VLI(Call, Effect::Any, EnvAccess::Leak) {
  public:
    Value* cls() { return arg(1); }
    Value** callArgs() { return &args()[2]; }
    const PirType* callTypes() { return &types()[2]; }
    size_t nCallArgs() { return nargs() - 2; }

    Call(Value* e, Value* fun, const std::vector<Value*>& args)
        : VarLenInstruction(PirType::valOrLazy(), e) {
        this->push_arg(RType::closure, fun);
        for (unsigned i = 0; i < args.size(); ++i)
            this->push_arg(RType::prom, args[i]);
    }

    void eachCallArg(arg_iterator it) {
        for (size_t i = 0; i < nCallArgs(); ++i) {
            Value* v = callArgs()[i];
            PirType t = callTypes()[i];
            it(v, t);
        }
    }
};

typedef SEXP (*CCODE)(SEXP, SEXP, SEXP, SEXP);

class VLI(CallBuiltin, Effect::Any, EnvAccess::Write) {
  public:
    const CCODE builtin;
    Value** callArgs() { return &args()[1]; }
    const PirType* callTypes() { return &types()[1]; }
    size_t nCallArgs() { return nargs() - 1; }

    CallBuiltin(Value* e, CCODE builtin, const std::vector<Value*>& args)
        : VarLenInstruction(PirType::valOrLazy(), e), builtin(builtin) {
        for (unsigned i = 0; i < args.size(); ++i)
            this->push_arg(PirType::val(), args[i]);
    }

    void eachCallArg(arg_iterator it) {
        for (size_t i = 0; i < nCallArgs(); ++i) {
            Value* v = callArgs()[i];
            PirType t = callTypes()[i];
            it(v, t);
        }
    }
};

class VLI(MkEnv, Effect::None, EnvAccess::Read) {
    std::vector<SEXP> varName;

  public:
    typedef std::function<void(SEXP name, Value* val)> local_it;

    void eachLocalVar(local_it it) {
        for (size_t i = 1; i < nargs(); ++i)
            it(varName[i - 1], arg(i));
    }

    MkEnv(Value* parent, const std::vector<SEXP>& names, Value** args)
        : VarLenInstruction(RType::env, parent), varName(names) {
        for (unsigned i = 0; i < varName.size(); ++i)
            this->push_arg(RType::prom, args[i]);
    }

    void printRhs(std::ostream& out) override;
};

class VLI(Phi, Effect::None, EnvAccess::None) {
  public:
    Phi() : VarLenInstruction(PirType::any()) {}
    void updateType();
};

#undef VLI
}
}

#endif
