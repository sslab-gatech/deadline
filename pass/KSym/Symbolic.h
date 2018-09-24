#ifndef SYMBOLIC_H_
#define SYMBOLIC_H_

#include "Project.h"

// memory layout
#define PAGE_SIZE           4096                  // 4KB

#define STACK_SLOT_LEN      1 * PAGE_SIZE         // 4KB
#define STACK_SLOT_NUM      128

#define EXTERN_SLOT_LEN     1024 * PAGE_SIZE      // 4MB
#define EXTERN_SLOT_NUM     128

#define HEAP_SLOT_LEN       1024 * PAGE_SIZE      // 4MB
#define HEAP_SLOT_NUM       128

// derived
#define STACK_BASE          PAGE_SIZE + STACK_SLOT_LEN
#define STACK_TERM          STACK_BASE + STACK_SLOT_LEN * STACK_SLOT_NUM

#define EXTERN_BASE         STACK_TERM + EXTERN_SLOT_LEN
#define EXTERN_TERM         EXTERN_BASE + EXTERN_SLOT_LEN * EXTERN_SLOT_NUM

#define HEAP_BASE           EXTERN_TERM + HEAP_SLOT_LEN
#define HEAP_TERM           HEAP_BASE + HEAP_SLOT_LEN * HEAP_SLOT_NUM

// forwared declerations
class SENode;
class SEGraph;

// classes
struct SymExpr {
  Z3_ast expr;

  SymExpr()
    : expr(nullptr) {}

  SymExpr(const SymExpr &other)
    : expr(other.expr) {}

  void simplify(Z3_context ctxt) {
    expr = Z3_simplify(ctxt, expr);
  }

  void dump(Z3_context ctxt) {
    errs() << "expr: ";
    DUMP.typedExpr(ctxt, expr);
  }
};

struct SymCond {
  vector<Z3_ast> conds;

  SymCond()
    : conds() {}

  SymCond(const SymCond &other)
    : conds(other.conds) {}

  void simplify(Z3_context ctxt) {
    for(unsigned i = 0; i < conds.size(); i++){
      conds[i] = Z3_simplify(ctxt, conds[i]);
    }
  }

  void dump(Z3_context ctxt) {
    errs() << "cond: [\n";
    for(Z3_ast c : conds) {
      errs() << "\t";
      DUMP.typedExpr(ctxt, c);
    }
    errs() << "]\n";
  }
};

class SymVal {
  // represent a single value and the conditions it follows 
  public:
    SymVal(SymExpr &e, SymCond &c) 
      : expr(e), cond(c) {}

    ~SymVal() {}

    SymExpr &getExpr() {
      return expr;
    }

    SymCond &getCond() {
      return cond;
    }

    bool ready() {
      return expr.expr != nullptr;
    }

    void simplify(Z3_context ctxt) {
      expr.simplify(ctxt);
      cond.simplify(ctxt);
    }

    void dump(Z3_context ctxt) {
      expr.dump(ctxt);
      cond.dump(ctxt);
    }

  protected:
    SymExpr expr;
    SymCond cond;
};

class SymVar {
  // represent all possible values of a variable
  public:
    SymVar() {} 

    ~SymVar() {
      for(SymVal *i : vals) {
        delete i;
      }
    }

    void add(SymVal *i) {
      vals.insert(i);
    }

    typedef typename set<SymVal *>::iterator iterator;

    iterator begin() {
      return vals.begin();
    }

    iterator end() {
      return vals.end();
    }

    SymVal *getSingleVal() {
      assert(vals.size() == 1);
      return *vals.begin();
    }

    bool ready() {
      return getSingleVal()->ready();
    }

    void simplify(Z3_context ctxt) {
      SymVal *v = getSingleVal();
      if(v->ready()){
        v->simplify(ctxt);
      }
    }

    void dump(Z3_context ctxt) {
      SymVal *v = getSingleVal();
      if(v->ready()){
        v->dump(ctxt);
      }
    }

  protected:
    set<SymVal *> vals;
};

enum CheckResult {
  // solver return
  CK_SAT          = 0,
  CK_UNDEF        = 1,
  CK_UNSAT        = 2,
  // symbllization
  CK_UNREL        = 3,
  CK_SYMERR       = 4
};

class SymExec {
  public:
    SymExec(ModuleOracle &m);

    ~SymExec();

    // symbol
    Z3_symbol newSymbol() {
      return Z3_mk_int_symbol(ctxt, varcount++);
    }

    // machine integer sorts
    Z3_sort getPointerSort() {
      return SORT_Pointer;
    }

    Z3_sort getMachIntSort(unsigned width) {
      auto i = sints.find(width);
      if(i != sints.end()){
        return i->second;
      } 
      
      Z3_sort res = Z3_mk_bv_sort(ctxt, width);
      sints.insert(make_pair(width, res));
      return res;
    }

    // sort checking
    Z3_sort getExprSort(Z3_ast expr) {
      return Z3_get_sort(ctxt, expr);
    }

    unsigned getExprSortWidth(Z3_ast expr) {
      Z3_sort srt = getExprSort(expr);
      assert(Z3_get_sort_kind(ctxt, srt) == Z3_BV_SORT);
      return Z3_get_bv_sort_size(ctxt, srt);
    }

    unsigned getExprSortSize(Z3_ast expr) {
      return getExprSortWidth(expr) / mo.getBits();
    }

    bool isBoolSort(Z3_ast expr) {
      return Z3_get_sort_kind(ctxt, getExprSort(expr)) == Z3_BOOL_SORT;
    }

    bool isIntSort(Z3_ast expr) {
      return Z3_get_sort_kind(ctxt, getExprSort(expr)) == Z3_INT_SORT;
    }

    bool isMachIntSort(Z3_ast expr) {
      return Z3_get_sort_kind(ctxt, getExprSort(expr)) == Z3_BV_SORT;
    }

    bool isPointerSort(Z3_ast expr) {
      return getExprSort(expr) == SORT_Pointer;
    }

    bool isMemSlotSort(Z3_ast expr) {
      return getExprSort(expr) == SORT_MemSlot;
    }

    bool isSameSort(Z3_ast expr1, Z3_ast expr2) {
      return getExprSort(expr1) == getExprSort(expr2);
    }

    bool isSameMachIntSort(Z3_ast expr1, Z3_ast expr2) {
      return isSameSort(expr1, expr2) && isMachIntSort(expr1);
    }

    bool isSameBoolSort(Z3_ast expr1, Z3_ast expr2) {
      return isSameSort(expr1, expr2) && isBoolSort(expr1);
    }

    // constant creation 
    Z3_ast createConstBool(bool val) {
      return val ? Z3_mk_true(ctxt) : Z3_mk_false(ctxt);
    }

    Z3_ast createConstMachIntUnsigned(__uint64 val, unsigned width) {
      return Z3_mk_unsigned_int64(ctxt, val, getMachIntSort(width));
    }

    Z3_ast createConstMachIntSigned(__int64 val, unsigned width) {
      return Z3_mk_int64(ctxt, val, getMachIntSort(width));
    }

    Z3_ast createConstMachIntBool(bool val, unsigned width) {
      return val
        ? createConstMachIntSigned(-1, width)
        : createConstMachIntSigned(0, width);
    }

    Z3_ast createConstPointer(uintptr_t val) {
      return Z3_mk_unsigned_int64(ctxt, val, SORT_Pointer);
    }

    Z3_ast createConstNull() {
      return createConstPointer(0);
    }

    // special creation
    Z3_ast createSpecialNone() {
      return Z3_mk_string(ctxt, "NONE");
    }

    // llvm linkage
    Z3_ast createConstTypeSize(Type *ty) {
      return createConstPointer(mo.getTypeSize(ty));
    }

    Z3_ast createConstTypeWidth(Type *ty) {
      return createConstPointer(mo.getTypeWidth(ty));
    }

    Z3_ast createConstTypeOffset(Type *ty, unsigned fid) {
      return createConstPointer(mo.getTypeOffset(ty, fid));
    }

    Z3_ast createConstMachInt(ConstantInt *ci) {
      return createConstMachIntUnsigned(ci->getZExtValue(), ci->getBitWidth());
    }

    Z3_ast createConstPointerFromInt(ConstantInt *ci) {
      return createConstPointer(ci->getZExtValue());
    }

    void prepareCalcOperation(
        Type *lty, Z3_ast lexp, 
        Type *rty, Z3_ast rexp) {

      uint64_t lw = mo.getTypeWidth(lty);
      uint64_t rw = mo.getTypeWidth(rty);

      assert(lw == rw && 
          lw == getExprSortWidth(lexp) && rw == getExprSortWidth(rexp));
    }

    void prepareCmpOperation(
        Type *lty, Z3_ast lexp, 
        Type *rty, Z3_ast rexp) {

      uint64_t lw = mo.getTypeWidth(lty);
      uint64_t rw = mo.getTypeWidth(rty);

      assert(lw == rw && 
          lw == getExprSortWidth(lexp) && rw == getExprSortWidth(rexp));
    }

    // variable creation
    Z3_ast createVarMachInt(unsigned width) {
      return Z3_mk_const(ctxt, newSymbol(), getMachIntSort(width));
    }

    Z3_ast createVarPointer() {
      return Z3_mk_const(ctxt, newSymbol(), getPointerSort());
    }

    // memory model
    Z3_ast incAndRetPtr(Z3_ast &ptr, unsigned len) {
      Z3_ast res = ptr;

      ptr = Z3_mk_bvadd(ctxt, ptr, createConstPointer(len));
      ptr = Z3_simplify(ctxt, ptr);

      return res;
    }

    Z3_ast createPtrStack() {
      return incAndRetPtr(sptr, STACK_SLOT_LEN);
    }

    Z3_ast createPtrExtern() {
      return incAndRetPtr(eptr, EXTERN_SLOT_LEN);
    }

    Z3_ast createPtrHeap() {
      return incAndRetPtr(hptr, HEAP_SLOT_LEN);
    }

    Z3_ast loadMemory(Z3_ast addr, unsigned size) {
      assert(isPointerSort(addr) && size != 0);
      Z3_ast res = Z3_mk_select(ctxt, memory, addr);

      Z3_ast idx, cur, par;
      for(unsigned i = 1; i < size; i++){
        idx = createConstPointer(i);
        cur = Z3_mk_bvadd(ctxt, addr, idx);
        par = Z3_mk_select(ctxt, memory, cur);
        res = Z3_mk_concat(ctxt, par, res);
      }

      return res;
    }

    void storeMemory(Z3_ast addr, Z3_ast expr) {
      // get the size to be overriden
      unsigned size = getExprSortSize(expr);

      // override the memory
      Z3_ast idx, cur, par;
      for(unsigned i = 0; i < size; i++){
        idx = createConstPointer(i);
        cur = Z3_mk_bvadd(ctxt, addr, idx);
        par = Z3_mk_extract(ctxt, 
            (i + 1) * mo.getBits() - 1, i * mo.getBits(), 
            expr);

        memory = Z3_mk_store(ctxt, memory, cur, par);
      }
    }

    // casts
    Z3_ast castMachIntTrunc(Z3_ast expr, unsigned width) {
      return Z3_mk_extract(ctxt, width - 1, 0, expr);
    }

    Z3_ast castMachIntZExt(Z3_ast expr, unsigned width) {
      unsigned sz = getExprSortWidth(expr);
      return Z3_mk_zero_ext(ctxt, width - sz, expr);
    }

    Z3_ast castMachIntSExt(Z3_ast expr, unsigned width) {
      unsigned sz = getExprSortWidth(expr);
      return Z3_mk_sign_ext(ctxt, width - sz, expr);
    }

    Z3_ast castBoolToMachInt(Z3_ast expr, unsigned width) {
      assert(isBoolSort(expr));
      return Z3_mk_ite(ctxt, expr,
          createConstMachIntSigned(-1, width),
          createConstMachIntSigned(0, width));
    }

    Z3_ast castBoolToMachIntInversed(Z3_ast expr, unsigned width) {
      assert(isBoolSort(expr));
      return Z3_mk_ite(ctxt, expr,
          createConstMachIntSigned(0, width),
          createConstMachIntSigned(-1, width));
    }

    Z3_ast castMachIntToBool(Z3_ast expr) {
      unsigned sz = getExprSortWidth(expr);
      return Z3_mk_not(ctxt, 
          Z3_mk_eq(ctxt, expr, createConstMachIntSigned(0, sz)));
    }

    Z3_ast castMachIntToBoolInversed(Z3_ast expr) {
      unsigned sz = getExprSortWidth(expr);
      return Z3_mk_eq(ctxt, expr, createConstMachIntSigned(0, sz));
    }

    Z3_ast castMachIntSwap(Z3_ast expr) {
      unsigned sz = getExprSortSize(expr);
      assert(sz % 2 == 0);
      Z3_ast res = Z3_mk_extract(ctxt, mo.getBits() - 1, 0, expr);

      Z3_ast par;
      for(unsigned i = 1; i < sz; i++){
        par = Z3_mk_extract(ctxt, 
            (i + 1) * mo.getBits() - 1, i * mo.getBits(),
            expr);

        res = Z3_mk_concat(ctxt, res, par);
      }

      return res;
    }

    // solver
    void addAssert(Z3_ast expr) {
      Z3_solver_assert(ctxt, solver, expr);
    }

    Z3_model getModel() {
      return Z3_solver_get_model(ctxt, solver);
    }

    // context
    Z3_context getContext() {
      return ctxt;
    }

    ModuleOracle &getOracle() {
      return mo;
    }

    // SENode getter
    SymVar *getOrCreateVar(SENode *node, bool &exist) {
      auto i = cache.find(node);
      if(i != cache.end()){
        exist = true;
        return i->second;
      }

      exist = false;
      SymVar *var = new SymVar();
      cache.insert(make_pair(node, var));
      return var;
    }

    SymVar *getVar(SENode *node) {
      auto i = cache.find(node);
      if(i == cache.end()){
        return nullptr;
      } else {
        return i->second;
      }
    }

    void simplify() {
      for(auto &i : cache){
        i.second->simplify(ctxt);
      }
    }

    void complete();

    // checks
    CheckResult checkOverlap(
        SymVar *src1, SymVar *len1,
        SymVar *src2, SymVar *len2);

    Z3_lbool checkOverlap(
        Z3_ast s1, Z3_ast l1, 
        Z3_ast s2, Z3_ast l2);

  protected:
    // module info
    ModuleOracle &mo;

    // z3 solver
    Z3_context ctxt;
    Z3_solver solver;

    // list of used sorts
    Z3_sort SORT_Pointer;
    Z3_sort SORT_MemSlot;
    Z3_sort SORT_MemBlob;

    // symbol counter
    int varcount;

    // the memory model
    Z3_ast memory;

    Z3_ast sptr;
    Z3_ast eptr;
    Z3_ast hptr;

    // machine integer sorts
    map<unsigned, Z3_sort> sints;

    // caches
    map<SENode *, SymVar *> cache;
};

#endif /* SYMBOLIC_H_ */
