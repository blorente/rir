#ifndef COMPILER_INSTRUCTION_LIST_H
#define COMPILER_INSTRUCTION_LIST_H

#define COMPILER_INSTRUCTIONS(V)                                               \
    V(LdFun)                                                                   \
    V(LdVar)                                                                   \
    V(LdConst)                                                                 \
    V(LdArg)                                                                   \
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
    V(CallBuiltin)                                                             \
    V(CallSafeBuiltin)                                                         \
    V(MkEnv)                                                                   \
    V(Lte)                                                                     \
    V(Gte)                                                                     \
    V(LAnd)                                                                    \
    V(LOr)                                                                     \
    V(Mod)                                                                     \
    V(Add)                                                                     \
    V(Div)                                                                     \
    V(Colon)                                                                   \
    V(Pow)                                                                     \
    V(Sub)                                                                     \
    V(Mul)                                                                     \
    V(Inc)                                                                     \
    V(Not)                                                                     \
    V(Lt)                                                                      \
    V(Gt)                                                                      \
    V(Neq)                                                                     \
    V(Eq)                                                                      \
    V(Length)                                                                  \
    V(IndexAccess)                                                             \
    V(IndexWrite)                                                              \
    V(Force)

#endif
