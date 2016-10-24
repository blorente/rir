#ifndef RIR_INTERPRETER_DATA_C_H
#define RIR_INTERPRETER_DATA_C_H

#include <stdint.h>
#include <assert.h>
#include "../config.h"

#include "R/r.h"


#ifdef __cplusplus
extern "C" {
#else
#define bool int
#define true 1
#define false 0
#endif

#ifdef ENABLE_SLOWASSERT
#define SLOWASSERT(what) assert(what)
#else
#define SLOWASSERT(what)                                                       \
    {}
#endif

// TODO force inlinine for clang & gcc
#define INLINE __attribute__((always_inline)) inline static

// Function magic constant is designed to help to distinguish between Function
// objects and normal INTSXPs. Normally this is not necessary, but a very
// creative user might try to assign arbitrary INTSXP to a closure which we
// would like to spot. Of course, such a creative user might actually put the
// magic in his vector too...
#define FUNCTION_MAGIC (unsigned)0xCAFEBABE

// Offset between function SEXP and Function* struct
// This is basically sizeof(SEXPREC_ALIGN)
#define FUNCTION_OFFSET 40

// Code magic constant is intended to trick the GC into believing that it is
// dealing with already marked SEXP.
// Note: gcgen needs to be 1, otherwise the write barrier will trigger
//  It also makes the SEXP look like no other SEXP (31) so that we can determine
//  whether a standard promise execution, or rir promise should be executed.
#define CODE_MAGIC (unsigned)0x110000ff

/** How many bytes do we need to align on 4 byte boundary?
 */
INLINE unsigned pad4(unsigned sizeInBytes) {
    unsigned x = sizeInBytes % 4;
    return (x != 0) ? (sizeInBytes + 4 - x) : sizeInBytes;
}

// we cannot use specific sizes for enums in C
typedef uint8_t OpcodeT;

/** Any argument to BC must be the size of this type. */
// TODO add static asserts somewhere in C++
typedef uint32_t ArgT;

// type  for constant & ast pool indices
typedef uint32_t Immediate;

// type  signed immediate values (unboxed ints)
typedef uint32_t SignedImmediate;

// type of relative jump offset (all jumps are relative)
typedef int32_t JumpOffset;

typedef unsigned FunctionIndex;
typedef unsigned ArgumentsCount;

// enums in C are not namespaces so I am using OP_ to disambiguate
typedef enum {
#define DEF_INSTR(name, ...) name,
#include "ir/insns.h"
    numInsns_
} Opcode;

/**
 * Aliases for readability.
 */
typedef SEXP FunctionSEXP;
typedef SEXP ClosureSEXP;
typedef SEXP PromiseSEXP;
typedef SEXP IntSEXP;

// type of relative jump offset (all jumps are relative)
typedef int32_t JumpOffset;

struct Function; // Forward declaration

// all sizes in bytes,
// length in element sizes

// ============
// Please do not change those without also changing how they are handled in the
// CodeEditor
// Indicates an argument is missing
#define MISSING_ARG_IDX ((unsigned)-1)
// Indicates an argument does not correspond to a valid CodeObject
#define DOTS_ARG_IDX ((unsigned)-2)
// Maximum valid entry for a CodeObject offset/idx entry 
#define MAX_ARG_IDX ((unsigned)-3)

/**
 * Code holds a sequence of instructions; for each instruction
 * it records the index of the source AST. Code is part of a
 * Function.
 *
 * Code objects are allocated contiguously within the data
 * section of a Function. The Function header can be found,
 * at an offset from the start of each Code object
 *
 * Instructions are variable size; Code knows how many bytes
 * are required for instructions.
 *
 * The number of indices of source ASTs stored in Code equals
 * the number of instructions.
 *
 * Instructions and AST indices are allocated one after the
 * other in the Code's data section with padding to ensure
 * alignment of indices.
 */
#pragma pack(push)
#pragma pack(1)

typedef struct Code {
    unsigned magic; ///< Magic number that attempts to be PROMSXP already marked
    /// by the GC

    unsigned header; /// offset to Function object

    // TODO comment these
    unsigned src; /// AST of the function (or promise) represented by the code

    unsigned stackLength; /// Number of slots in stack required

    unsigned codeSize; /// bytes of code (not padded)

    unsigned skiplistLength; /// number of skiplist entries

    unsigned srcLength; /// number of instructions

    uint32_t* callSites;

    uint8_t data[]; /// the instructions
} Code;
#pragma pack(pop)

// Accessors to the call site
INLINE uint32_t* CallSite_call(uint32_t* callSite) { return callSite; }
INLINE uint32_t* CallSite_hasNames(uint32_t* callSite) { return callSite + 1; }
INLINE uint32_t* CallSite_args(uint32_t* callSite) { return callSite + 2; }
INLINE uint32_t* CallSite_names(uint32_t* callSite, uint32_t nargs) {
    assert(*CallSite_hasNames(callSite));
    return callSite + 2 + nargs;
}
INLINE uint32_t* CallSite_selector(uint32_t* callSite, uint32_t nargs) {
    bool hasNames = *CallSite_hasNames(callSite);
    return callSite + 2 + nargs * (hasNames ? 2 : 1);
}

/** Returns whether the SEXP appears to be valid promise, i.e. a pointer into
 * the middle of the linearized code.
 */
INLINE Code * isValidCodeObject(SEXP what) {
    unsigned x = *(unsigned*)what;
    if (x == CODE_MAGIC)
        return (Code*)what;
    else
        return nullptr;
}

/** Returns a pointer to the instructions in c.  */
INLINE OpcodeT* code(Code* c) { return (OpcodeT*)c->data; }

/** Returns a pointer to the source AST indices in c.  */
INLINE unsigned* skiplist(Code* c) {
    return (unsigned*)(c->data + pad4(c->codeSize));
}

/** Returns a pointer to the source AST indices in c.  */
INLINE unsigned* raw_src(Code* c) {
    return (unsigned*)(c->data + pad4(c->codeSize) +
                       c->skiplistLength * 2 * sizeof(unsigned));
}

/** Moves the pc to next instruction, based on the current instruction length
 */
INLINE OpcodeT* advancePc(OpcodeT* pc) {
    switch (*pc++) {
#define DEF_INSTR(name, imm, ...)                                              \
    case name:                                                                 \
        pc += sizeof(ArgT) * imm;                                              \
        break;
#include "ir/insns.h"
    default:
        assert(false && "Unknown instruction");
    }
    return pc;
}

INLINE unsigned getSrcIdxAt(Code* c, OpcodeT* pc, bool allowMissing) {

    unsigned* sl = skiplist(c);
    unsigned sl_i = 0;
    OpcodeT* start = c->data;

    SLOWASSERT(allowMissing || *sl <= pc - start);

    if (sl[0] > pc - start)
        return 0;

    while (sl[sl_i] <= pc - start && sl_i < 2 * c->skiplistLength)
        sl_i += 2;

    // we need to determine index of the current instruction
    OpcodeT* x = code(c) + sl[sl_i - 2];
    // find the pc of the current instructions
    unsigned insIdx = sl[sl_i - 1];

    while (x != pc) {
        x = advancePc(x);
        ++insIdx;
        if (insIdx == c->srcLength) {
            SLOWASSERT(allowMissing);
            return 0;
        }
    }
    unsigned sidx = raw_src(c)[insIdx];
    SLOWASSERT(allowMissing || sidx);

    return sidx;
}

/** Returns the next Code in the current function. */
INLINE Code* next(Code* c) {
    return (Code*)(c->data + pad4(c->codeSize) +
                   c->srcLength * sizeof(unsigned) +
                   c->skiplistLength * 2 * sizeof(unsigned));
}

/** Returns a pointer to the Function to which c belongs. */
INLINE struct Function* function(Code* c) {
    return (struct Function*)((uint8_t*)c - c->header);
}

// TODO thats a bit nasty
INLINE SEXP functionStore(struct Function* f) {
    return (SEXP)((uintptr_t)f - FUNCTION_OFFSET);
}

// TODO removed src reference, now each code has its own

/** A Function holds the RIR code for some GNU R function.
 *  Each function start with a header and a sequence of
 *  Code objects for the body and all of the promises
 *  in the code.
 *
 *  The header start with a magic constant. This is a
 *  temporary hack so that it is possible to differentiate
 *  an R int vector from a Function. Eventually, we will
 *  add a new SEXP type for this purpose.
 *
 *  The size of the function, in bytes, includes the size
 *  of all of its Code objects and is padded to a word
 *  boundary.
 *
 *  A Function may be the result of optimizing another
 *  Function, in which case the origin field stores that
 *  Function as a SEXP pointer.
 *
 *  A Function has a source AST, stored in src.
 *
 *  A Function has a number of Code objects, codeLen, stored
 *  inline in data.
 */
#pragma pack(push)
#pragma pack(1)
struct Function {
    unsigned magic; /// used to detect Functions 0xCAFEBABE

    unsigned size; /// Size, in bytes, of the function and its data

    unsigned invocationCount;

    FunctionSEXP
        origin; /// Same Function with fewer optimizations, NULL if original

    unsigned codeLength; /// number of Code objects in the Function

    // We can get to this by searching, but this isfaster and so worth the extra
    // four bytes
    unsigned foffset; ///< Offset to the code of the function (last code)

    uint8_t data[]; // Code objects stored inline
};
#pragma pack(pop)

typedef struct Function Function;

INLINE Function * isValidFunctionObject(SEXP s) {
    if (TYPEOF(s) != INTSXP)
        return NULL;
    if ((unsigned)Rf_length(s)*sizeof(int) < sizeof(Function))
        return NULL;
    Function* f = (Function*)INTEGER(s);
    if (f->magic != FUNCTION_MAGIC)
        return NULL;
    if (f->size > (unsigned)Rf_length(s)*sizeof(int))
        return NULL;
    if (f->foffset >= f->size - sizeof(Code))
        return NULL;
    // TODO it is still only an assumption
    return f;
}

/** Returns the INTSXP for the Function object. */
INLINE SEXP functionSEXP(Function * f) {
    SEXP result = (SEXP)((uint8_t*)f - sizeof(VECTOR_SEXPREC));
    assert(TYPEOF(result) == INTSXP && "Either wrong memory altogether or bad counting");
    return result;
}

INLINE Code* functionCode(Function* f) {
    return (Code*)((uintptr_t)f + f->foffset);
}

/** Returns the first code object associated with the function.
 */
INLINE Code* begin(Function* f) { return (Code*)f->data; }

/** Returns the end of the function as code object, for interation purposes.
 */
INLINE Code* end(Function* f) { return (Code*)((uint8_t*)f + f->size); }

/** Returns the code object with given offset */
INLINE Code* codeAt(Function* f, unsigned offset) {
    return (Code*)((uint8_t*)f + offset);
}

#ifdef __cplusplus
}
#endif

#endif // RIR_INTERPRETER_C_H
