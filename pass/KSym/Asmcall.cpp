#include "Project.h"

// expansion
#define ASMCALL_DERIVE_0(name)                                    \
  SymVal *SEOpvAsm_##name::derive(SymExec &sym)

#define ASMCALL_DERIVE_1(name, arg0)                              \
  SymVal *SEOpvAsm_##name::derive(SymExec &sym,                   \
      SymVal *arg0)

#define ASMCALL_DERIVE_2(name, arg0, arg1)                        \
  SymVal *SEOpvAsm_##name::derive(SymExec &sym,                   \
      SymVal *arg0, SymVal *arg1)

#define ASMCALL_DERIVE_3(name, arg0, arg1, arg2)                  \
  SymVal *SEOpvAsm_##name::derive(SymExec &sym,                   \
      SymVal *arg0, SymVal *arg1, SymVal *arg2)

// helpers
#define CHECK_INST                                                \
  assert(isa<CallInst>(inst));                                    \
  CallInst *op = cast<CallInst>(inst);

#define GET_ORACLE                                                \
  ModuleOracle &oracle = sym.getOracle();

#define INIT_EXPR                                                 \
  SymExpr expr;                                                   \
  SymCond cond;

#define FINI_EXPR                                                 \
  return new SymVal(expr, cond);

#define IGNORED                                                   \
  INIT_EXPR;                                                      \
  FINI_EXPR;

// asm call symbolization routines

// fetch
ASMCALL_DERIVE_2(__get_user_n, src, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createVarPointer();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}
