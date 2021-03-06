#ifndef RJIT_RIR_BC_INC
#define RJIT_RIR_BC_INC

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cstring>

#include "R/r.h"

#include <vector>

#include "interpreter/interp_data.h"

namespace rir {

/* ============================================================================
 * ==== Bytecode
 *
 * This file contains all definitions concerning Bytecodes.
 * Bytecodes are variable length, they all start with a BC_t type tag which is
 * followed by a variable sized immediate argument.
 *
 * The BC class contains constructor methods and all the infrastructure needed
 * to decode/write bytecodes to/from a byte stream.
 *
 * To add a bytecode:
 * 1. add it to BC_t
 * 2. add a constructor method to class BC
 * 3. add an entry to immediate_size in BC.h
 * 4. implement it (ie. resolve all switch statement warnings)
 */

// ============================================================
// ==== BC types
//
enum class BC_t : OpcodeT {

#define DEF_INSTR(name, ...) name,
#include "insns.h"

    // A label/jump target (used internally by CodeEditor only!)
    label,

    num_of

};

// TODO the immediate argument types should follow the C immediate types

// ============================================================
// ==== immediate argument types
//
#pragma pack(push)
#pragma pack(0)

// index into the constant pool
typedef uint32_t pool_idx_t;
// index into a functions array of code objects
typedef uint32_t fun_idx_t;
// number of arguments
typedef uint32_t num_args_t;
// jmp offset
typedef int32_t jmp_t;
typedef jmp_t Label;
typedef struct {
    uint32_t call_id;
    uint32_t nargs;
} CallArgs;
typedef struct {
    uint32_t name;
    uint32_t expected;
    uint32_t id;
} GuardFunArgs;
#pragma pack(pop)

static constexpr size_t MAX_NUM_ARGS = 1L << (8 * sizeof(pool_idx_t));
static constexpr size_t MAX_POOL_IDX = 1L << (8 * sizeof(pool_idx_t));
static constexpr size_t MAX_JMP = (1L << ((8 * sizeof(jmp_t)) - 1)) - 1;
static constexpr size_t MIN_JMP = -(1L << ((8 * sizeof(jmp_t)) - 1));

// ============================================================
// ==== Creation and decoding of Bytecodes
//
// * Static factory functions to create an instance of a bytecode
//   which can be pushed onto a CodeStream
// * read and advance to read the next bytecode from an array

class CallSite;
class CodeStream;
class BC {
  public:
    // This is only used internally in the BC handle objects
    // On the bytecode stream each immediate argument uses only the actual
    // space required.
    union immediate_t {
        CallArgs call_args;
        GuardFunArgs guard_fun_args;
        uint32_t guard_id;
        pool_idx_t pool;
        fun_idx_t fun;
        num_args_t arg_idx;
        jmp_t offset;
        uint32_t i;
    };

    BC() : bc(BC_t::invalid_), immediate({{0}}) {}
    BC operator=(BC other) {
        bc = other.bc;
        immediate = other.immediate;
        return other;
    }

    bool operator==(const BC& other) const;

    bool is(BC_t aBc) { return bc == aBc; }

    BC_t bc;
    immediate_t immediate;

    inline size_t size() { return size(bc); }
    inline size_t popCount() {
        // return also is a leave
        assert(bc != BC_t::return_);
        if (bc == BC_t::call_stack_)
            return immediate.call_args.nargs + 1;
        if (bc == BC_t::static_call_stack_ || bc == BC_t::dispatch_stack_)
            return immediate.call_args.nargs;
        return popCount(bc);
    }
    inline size_t pushCount() { return pushCount(bc); }

    // Used to serialize bc to CodeStream
    void write(CodeStream& cs) const;

    // Print it to stdout
    void print();
    void print(CallSite cs);
    void printArgs(CallSite cs);
    void printNames(CallSite cs);
    void printProfile(CallSite cs);

    // Accessors to load immediate constant from the pool
    SEXP immediateConst();

    // Return the callsite of this BC, needs the cassSites buffer as input
    CallSite callSite(Code* code);

    inline static BC_t* jmpTarget(BC_t* pos) {
        BC bc = BC::decode(pos);
        assert(bc.isJmp());
        return (BC_t*)((uintptr_t)pos + bc.size() + bc.immediate.offset);
    }

    bool isCallsite() {
        return bc == BC_t::call_ || bc == BC_t::dispatch_ ||
               bc == BC_t::call_stack_ || bc == BC_t::dispatch_stack_ ||
               bc == BC_t::static_call_stack_;
    }

    bool hasPromargs() {
        return bc == BC_t::call_ || bc == BC_t::dispatch_ ||
               bc == BC_t::promise_ || bc == BC_t::push_code_;
    }

    bool isJmp() {
        return bc == BC_t::br_ || bc == BC_t::brtrue_ || bc == BC_t::brfalse_ ||
               bc == BC_t::brobj_ || bc == BC_t::beginloop_;
    }

    bool isPure() { return isPure(bc); }

    bool isReturn() { return bc == BC_t::ret_ || bc == BC_t::return_; }

    bool isGuard() { return bc == BC_t::guard_fun_ || bc == BC_t::guard_env_; }

    // ==== BC decoding logic
    inline static BC advance(BC_t** pc);
    inline static BC decode(BC_t* pc);

    // ==== Factory methods
    // to create new BC objects, which can be streamed to a CodeStream
    inline static BC push(SEXP constant);
    inline static BC push(double constant);
    inline static BC push(int constant);
    inline static BC push_code(fun_idx_t i);
    inline static BC ldfun(SEXP sym);
    inline static BC ldvar(SEXP sym);
    inline static BC ldlval(SEXP sym);
    inline static BC ldarg(SEXP sym);
    inline static BC ldddvar(SEXP sym);
    inline static BC promise(fun_idx_t prom);
    inline static BC ret();
    inline static BC pop();
    inline static BC force();
    inline static BC asast();
    inline static BC stvar(SEXP sym);
    inline static BC missing(SEXP sym);
    inline static BC subassign();
    inline static BC subassign2(SEXP sym);
    inline static BC length();
    inline static BC names();
    inline static BC setNames();
    inline static BC alloc(int type);
    inline static BC asbool();
    inline static BC beginloop(jmp_t);
    inline static BC endcontext();
    inline static BC brtrue(jmp_t);
    inline static BC brfalse(jmp_t);
    inline static BC br(jmp_t);
    inline static BC brobj(jmp_t);
    inline static BC label(jmp_t);
    inline static BC lti();
    inline static BC eqi();
    inline static BC pushi(uint32_t);
    inline static BC push_argi();
    inline static BC dupi();
    inline static BC dup();
    inline static BC dup2();
    inline static BC testBounds();
    inline static BC inc();
    inline static BC close();
    inline static BC add();
    inline static BC mul();
    inline static BC div();
    inline static BC pow();
    inline static BC idiv();
    inline static BC mod();
    inline static BC sub();
    inline static BC lt();
    inline static BC seq();
    inline static BC uniq();
    inline static BC asLogical();
    inline static BC lglOr();
    inline static BC lglAnd();
    inline static BC guardName(SEXP, SEXP);
    inline static BC guardNamePrimitive(SEXP);
    inline static BC guardEnv(uint32_t id);
    inline static BC isfun();
    inline static BC invisible();
    inline static BC visible();
    inline static BC extract1();
    inline static BC subset1();
    inline static BC extract2();
    inline static BC subset2();
    inline static BC swap();
    inline static BC put(uint32_t);
    inline static BC pick(uint32_t);
    inline static BC pull(uint32_t);
    inline static BC is(uint32_t);
    inline static BC return_();
    inline static BC int3();

  private:
    explicit BC(BC_t bc) : bc(bc), immediate({{0}}) {}
    BC(BC_t bc, immediate_t immediate) : bc(bc), immediate(immediate) {}

    static unsigned size(BC_t bc) {
        switch (bc) {
#define DEF_INSTR(name, imm, opop, opush, pure)                                \
    case BC_t::name:                                                           \
        return imm * sizeof(ArgT) + 1;
#include "insns.h"
        default:
            return 0;
        }
    }

    static unsigned immCount(BC_t bc) {
        switch (bc) {
#define DEF_INSTR(name, imm, opop, opush, pure)                                \
    case BC_t::name:                                                           \
        return imm;
#include "insns.h"
        default:
            assert(false);
            return 0;
        }
    }

    static char const* name(BC_t bc) {
        switch (bc) {
#define DEF_INSTR(name, imm, opop, opush, pure)                                \
    case BC_t::name:                                                           \
        return #name;
#include "insns.h"
        default:
            return "???";
        }
    }

    static unsigned pushCount(BC_t bc) {
        switch (bc) {
#define DEF_INSTR(name, imm, opop, opush, pure)                                \
    case BC_t::name:                                                           \
        return opush;
#include "insns.h"
        default:
            assert(false);
            return 0;
        }
    }

    static unsigned popCount(BC_t bc) {
        switch (bc) {
#define DEF_INSTR(name, imm, opop, opush, pure)                                \
    case BC_t::name:                                                           \
        assert(opop != -1);                                                    \
        return opop;
#include "insns.h"
        default:
            assert(false);
            return 0;
        }
    }

    static unsigned isPure(BC_t bc) {
        switch (bc) {
#define DEF_INSTR(name, imm, opop, opush, pure)                                \
    case BC_t::name:                                                           \
        return pure;
#include "insns.h"
        case BC_t::label:
            return false;
        default:
            assert(false);
            return 0;
        }
    }

    friend class CodeEditor;
};

class CallSite {
  public:
    BC bc;
    CallSiteStruct* cs = nullptr;

    CallSite(){};
    CallSite(BC bc, CallSiteStruct* cs);

    bool isValid() { return cs != nullptr; }
    num_args_t nargs() { return cs->nargs; }
    SEXP call();
    const fun_idx_t* args() { return CallSite_args(cs); }
    fun_idx_t arg(num_args_t idx) { return CallSite_args(cs)[idx]; }
    bool hasNames() { return cs->hasNames; }
    bool hasTarget() { return cs->hasTarget; }
    bool hasProfile() { return cs->hasProfile; }
    bool hasImmediateArgs() { return cs->hasImmediateArgs; }
    SEXP selector();
    SEXP name(num_args_t idx);
    SEXP target();

    CallSiteProfile* profile() { return CallSite_profile(cs); }
};

} // rir

#endif
