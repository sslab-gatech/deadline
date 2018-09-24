#include "Project.h"

// expansion
#define LIBCALL_DERIVE_0(name)                                    \
  SymVal *SEOpvCall_##name::derive(SymExec &sym)

#define LIBCALL_DERIVE_1(name, arg0)                              \
  SymVal *SEOpvCall_##name::derive(SymExec &sym,                  \
      SymVal *arg0)

#define LIBCALL_DERIVE_2(name, arg0, arg1)                        \
  SymVal *SEOpvCall_##name::derive(SymExec &sym,                  \
      SymVal *arg0, SymVal *arg1)

#define LIBCALL_DERIVE_3(name, arg0, arg1, arg2)                  \
  SymVal *SEOpvCall_##name::derive(SymExec &sym,                  \
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

// libcall symbolization routines

// intrinsics
LIBCALL_DERIVE_1(llvm_objectsize_i64_p0i8, obj) {
  INIT_EXPR;

  CHECK_INST;

  Value *a0 = op->getArgOperand(0);
  if(GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(a0)){
    expr.expr = sym.createConstTypeSize(gep->getResultElementType());
  } else {
    expr.expr = sym.createVarPointer();

    sym.addAssert(Z3_mk_bvult(sym.getContext(), 
          expr.expr, sym.createConstPointer(HEAP_SLOT_LEN)));
  }

  FINI_EXPR;
}

LIBCALL_DERIVE_1(llvm_bswap_i16, iv) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.castMachIntSwap(iv->getExpr().expr);

  FINI_EXPR;
}

// malloc
LIBCALL_DERIVE_1(__get_free_pages, order) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  FINI_EXPR;
}

LIBCALL_DERIVE_1(__kmalloc, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_1(__kmalloc_track_caller, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_1(__vmalloc, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_0(get_zeroed_page) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  FINI_EXPR;
}

LIBCALL_DERIVE_0(kmem_cache_alloc) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  FINI_EXPR;
}

LIBCALL_DERIVE_1(krealloc, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_1(kvmalloc_node, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_1(vmalloc, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_1(vzalloc, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

// memory
LIBCALL_DERIVE_2(kmemdup, src, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

// fetch
LIBCALL_DERIVE_3(__copy_user_flushcache, dst, src, size) {
  INIT_EXPR;

  CHECK_INST;
  GET_ORACLE;

  expr.expr = sym.createConstMachIntUnsigned(0, 
      oracle.getTypeWidth(op->getType()));

  // NOTE: special treatment on 32bit size
  Z3_ast se = size->getExpr().expr;
  if(!sym.isPointerSort(se)){
    se = sym.castMachIntZExt(se, oracle.getPointerWidth());
  }

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        se, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        se, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_3(__copy_user_nocache, dst, src, size) {
  INIT_EXPR;

  CHECK_INST;
  GET_ORACLE;

  expr.expr = sym.createConstMachIntUnsigned(0, 
      oracle.getTypeWidth(op->getType()));

  // NOTE: special treatment on 32bit size
  Z3_ast se = size->getExpr().expr;
  if(!sym.isPointerSort(se)){
    se = sym.castMachIntZExt(se, oracle.getPointerWidth());
  }

  sym.addAssert(Z3_mk_bvugt(sym.getContext(), 
        se, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        se, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_3(_copy_from_user, dst, src, size) {
  INIT_EXPR;

  CHECK_INST;
  GET_ORACLE;

  expr.expr = sym.createConstMachIntUnsigned(0, 
      oracle.getTypeWidth(op->getType()));

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_2(memdup_user, src, size) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createPtrHeap();

  sym.addAssert(Z3_mk_bvugt(sym.getContext(),
        size->getExpr().expr, sym.createConstPointer(0)));
  sym.addAssert(Z3_mk_bvult(sym.getContext(), 
        size->getExpr().expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

// string
LIBCALL_DERIVE_3(memcmp, cs, ct, count) {
  INIT_EXPR;

  CHECK_INST;
  GET_ORACLE;

  expr.expr = sym.createConstMachIntUnsigned(0, 
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

LIBCALL_DERIVE_2(strcmp, cs, ct) {
  INIT_EXPR;

  CHECK_INST;
  GET_ORACLE;

  expr.expr = sym.createConstMachIntUnsigned(0, 
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

LIBCALL_DERIVE_1(strlen, buf) {
  INIT_EXPR;

  expr.expr = sym.createVarPointer();

  sym.addAssert(Z3_mk_bvult(sym.getContext(),
        expr.expr, sym.createConstPointer(HEAP_SLOT_LEN)));

  FINI_EXPR;
}

LIBCALL_DERIVE_3(strncmp, cs, ct, count) {
  INIT_EXPR;

  CHECK_INST;
  GET_ORACLE;

  expr.expr = sym.createConstMachIntUnsigned(0, 
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

LIBCALL_DERIVE_2(strnlen, buf, count) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createVarPointer();
  sym.addAssert(Z3_mk_bvule(sym.getContext(),
        expr.expr, count->getExpr().expr));

  FINI_EXPR;
}

LIBCALL_DERIVE_2(strnlen_user, buf, count) {
  INIT_EXPR;

  CHECK_INST;

  expr.expr = sym.createVarPointer();
  sym.addAssert(Z3_mk_bvule(sym.getContext(),
        expr.expr, count->getExpr().expr));

  FINI_EXPR;
}

// transfer
LIBCALL_DERIVE_0(_copy_to_user) {
  IGNORED;
}

LIBCALL_DERIVE_0(compat_alloc_user_space) {
  IGNORED;
}

// sync
LIBCALL_DERIVE_0(init_wait_entry) {
  IGNORED;
}

LIBCALL_DERIVE_0(finish_wait) {
  IGNORED;
}

LIBCALL_DERIVE_0(mutex_lock_interruptible_nested) {
  IGNORED;
}

LIBCALL_DERIVE_0(mutex_trylock) {
  IGNORED;
}

LIBCALL_DERIVE_0(prepare_to_wait_event) {
  IGNORED;
}

// info
LIBCALL_DERIVE_0(__dynamic_dev_dbg) {
  IGNORED;
}

LIBCALL_DERIVE_0(__dynamic_pr_debug) {
  IGNORED;
}

LIBCALL_DERIVE_0(printk) {
  IGNORED;
}

// checks
LIBCALL_DERIVE_0(__check_object_size) {
  IGNORED;
}

LIBCALL_DERIVE_0(__inode_permission) {
  IGNORED;
}

LIBCALL_DERIVE_0(__list_add_valid) {
  IGNORED;
}

LIBCALL_DERIVE_0(__virt_addr_valid) {
  IGNORED;
}

LIBCALL_DERIVE_0(capable) {
  IGNORED;
}

LIBCALL_DERIVE_0(kasan_check_write) {
  IGNORED;
}

LIBCALL_DERIVE_0(mnt_want_write_file) {
  IGNORED;
}

LIBCALL_DERIVE_0(ns_capable) {
  IGNORED;
}

LIBCALL_DERIVE_0(security_capable) {
  IGNORED;
}

LIBCALL_DERIVE_0(security_file_fnctl) {
  IGNORED;
}

LIBCALL_DERIVE_0(security_file_ioctl) {
  IGNORED;
}
