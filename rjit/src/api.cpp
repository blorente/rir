#include <Rinternals.h>
#include <llvm/IR/Module.h>

#include "Compiler.h"

#include "api.h"

using namespace rjit;


/** Compiles given ast and returns the NATIVESXP for it.
 */
REXPORT SEXP jitAst(SEXP ast) {
    Compiler c("module");
    SEXP result = c.compile("rfunction", ast);
    c.jitAll();
    return result;
}


/** More complex compilation method that compiles multiple functions into a specified module name.

  The module name is expected to be a STRSXP and the functions is expected to be a pairlist. If pairlist has tags associated with the elements, they will be used as function names.
 */
REXPORT SEXP jitFunctions(SEXP moduleName, SEXP functions) {
    char const * mName = CHAR(STRING_ELT(moduleName, 0));
    Compiler c(mName);
    while (functions != R_NilValue) {
        SEXP f = CAR(functions);
        // get the function ast
        SEXP body = BODY(f);
        SEXP name = TAG(functions);
        char const * fName = (name == R_NilValue) ? "unnamed function" : CHAR(PRINTNAME(name));
        if (TYPEOF(body) == BCODESXP)
            warning("Ignoring %s because it is in bytecode", fName);
        else if (TYPEOF(body) == NATIVESXP)
            warning("Ignoring %s because it is already compiled", fName);
        else
            SET_BODY(f, c.compileFunction(fName, body));
        // move to next function
        functions = CDR(functions);
    }
    c.jitAll();
    return moduleName;
}

/** Returns the constant pool associated with the given NATIVESXP.
 */
REXPORT SEXP jitConstants(SEXP expression) {
    assert(TYPEOF(expression) == NATIVESXP and "JIT constants can only be extracted from a NATIVESXP argument");
    return CDR(expression);
}

/** Displays the LLVM IR for given NATIVESXP.
 */
REXPORT SEXP jitLLVM(SEXP expression) {
    assert(TYPEOF(expression) == NATIVESXP and "LLVM code can only be extracted from a NATIVESXP argument");
    llvm::Function * f = reinterpret_cast<llvm::Function *>(TAG(expression));
    f->dump();
    return R_NilValue;
}