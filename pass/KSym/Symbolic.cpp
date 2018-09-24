#include "Project.h"

// main
SymExec::SymExec(ModuleOracle &m) 
  : mo(m) {

  // init Z3
  Z3_config cfg = Z3_mk_config();
  ctxt = Z3_mk_context(cfg);
  Z3_del_config(cfg);

  solver = Z3_mk_solver(ctxt);
  Z3_solver_inc_ref(ctxt, solver);

  // create sorts
  unsigned ptrsz = mo.getPointerWidth(); 
  SORT_Pointer = Z3_mk_bv_sort(ctxt, ptrsz);
  sints[ptrsz] = SORT_Pointer;

  SORT_MemSlot = Z3_mk_bv_sort(ctxt, mo.getBits());
  SORT_MemBlob = Z3_mk_array_sort(ctxt, SORT_Pointer, SORT_MemSlot);

  // symbol counter
  varcount = 0;

  // memory model
  memory = Z3_mk_const(ctxt,
      Z3_mk_string_symbol(ctxt, "memory"),
      SORT_MemBlob);

  sptr = Z3_mk_bvmul(ctxt, createVarPointer(), createConstPointer(PAGE_SIZE));
  addAssert(Z3_mk_bvuge(ctxt, sptr, createConstPointer(STACK_BASE)));

  eptr = Z3_mk_bvmul(ctxt, createVarPointer(), createConstPointer(PAGE_SIZE));
  addAssert(Z3_mk_bvuge(ctxt, eptr, createConstPointer(EXTERN_BASE)));

  hptr = Z3_mk_bvmul(ctxt, createVarPointer(), createConstPointer(PAGE_SIZE));
  addAssert(Z3_mk_bvuge(ctxt, hptr, createConstPointer(HEAP_BASE)));
}

SymExec::~SymExec() {
  // clean up cache
  for(auto const &i : cache){
    delete i.second;
  }

  // destroy z3
  Z3_solver_dec_ref(ctxt, solver);
  Z3_del_context(ctxt);
}

void SymExec::complete() {
  // memory model
  addAssert(Z3_mk_bvule(ctxt, sptr, createConstPointer(STACK_TERM)));
  addAssert(Z3_mk_bvule(ctxt, eptr, createConstPointer(EXTERN_TERM)));
  addAssert(Z3_mk_bvule(ctxt, hptr, createConstPointer(HEAP_TERM)));
}

// symbolize the SEG
void SEGraph::symbolize(SymExec &sym) {
  // symbolize all leaf and var nodes
  for(auto &i : nodes){
    SENode *node = i.second;
    if(isa<SENodeLeaf>(node) || isa<SENodeVar>(node)){
      node->getSymbol(sym);
    }
  }

  // iterate through the trace
  int len = trace.size();
  for(int c = 0; c < len; c++){
    Instruction *i = trace.at(c);

    // check if the stmt is in SEG
    SENode *node = getNodeOrNull(c, i);
    if(node == nullptr){
      continue;
    }

    // symbolize the node
    node->getSymbol(sym);
  }

  // finish up
  sym.complete();
}

static inline void replayNode(SENode *node, 
    SymExec &sym, Z3_context ctxt, Z3_model model) {

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_FUNC)
  SLOG.map();

  SLOG.log("inst", Helper::getValueRepr(node->getVal()));

  SymVar *svar = node->getSymbol(sym);
  if(svar == nullptr){
    SLOG.log("expr", "<unknown>");
    SLOG.log("eval", "<unknown>");
  } else {
    Z3_ast expr = svar->getSingleVal()->getExpr().expr;
    SLOG.log("expr", Helper::getExprRepr(ctxt, expr)); 

    Z3_ast eval;
    assert(Z3_model_eval(ctxt, model, expr, true, &eval) == Z3_TRUE);
    SLOG.log("eval", Helper::getExprRepr(ctxt, eval));
  }

  SLOG.pop();
#endif
}

void SEGraph::replay(SymExec &sym) {
  Z3_context ctxt = sym.getContext();
  Z3_model model = sym.getModel();

  // dump all var nodes
  for(auto &i : nodes){
    SENode *node = i.second;
    if(isa<SENodeVar>(node)){
      replayNode(node, sym, ctxt, model);
    }
  }

  // iterate through the trace
  int len = trace.size();
  for(int c = 0; c < len; c++){
    Instruction *i = trace.at(c);

    // check if the stmt is in SEG
    SENode *node = getNodeOrNull(c, i);
    if(node == nullptr){
      continue;
    }

    // replay the node
    replayNode(node, sym, ctxt, model);
  }
}

// checks
CheckResult SymExec::checkOverlap(
    SymVar *src1, SymVar *len1,
    SymVar *src2, SymVar *len2) {

  // make sure that their symbolic exprs exist
  if(src1 == nullptr || len1 == nullptr || src2 == nullptr || len2 == nullptr){
    return CK_SYMERR;
  }

  Z3_ast s1 = src1->getSingleVal()->getExpr().expr;
  Z3_ast l1 = len1->getSingleVal()->getExpr().expr;
  Z3_ast s2 = src2->getSingleVal()->getExpr().expr;
  Z3_ast l2 = len2->getSingleVal()->getExpr().expr;

  // NOTE: special treatment on 32bit size
  if(!isPointerSort(l1)){
    l1 = castMachIntZExt(l1, mo.getPointerWidth());
  }

  if(!isPointerSort(l2)){
    l2 = castMachIntZExt(l2, mo.getPointerWidth());
  }

  // the actual solver
  Z3_lbool result = checkOverlap(s1, l1, s2, l2);

  switch(result){
    case Z3_L_TRUE:
      return CK_SAT;

    case Z3_L_FALSE:
      return CK_UNSAT;

    case Z3_L_UNDEF:
      return CK_UNDEF;
  }

  llvm_unreachable("Should not reach here");
  return CK_SYMERR;
}

Z3_lbool SymExec::checkOverlap(
    Z3_ast s1, Z3_ast l1,
    Z3_ast s2, Z3_ast l2) {

  // sanity check
  assert(isPointerSort(s1) && isPointerSort(l1) && 
      isPointerSort(s2) && isPointerSort(l2));

  // save context
  Z3_solver_push(ctxt, solver);

  // condition: (s2 <= s1 < s2 + l2) || (s1 <= s2 < s1 + l1)
  Z3_ast d1 = Z3_mk_bvadd(ctxt, s1, l1);
  addAssert(Z3_mk_bvadd_no_overflow(ctxt, s1, l1, false));

  Z3_ast d2 = Z3_mk_bvadd(ctxt, s2, l2);
  addAssert(Z3_mk_bvadd_no_overflow(ctxt, s2, l2, false));

  Z3_ast cls[2];

  Z3_ast c1[2];
  c1[0] = Z3_mk_bvuge(ctxt, s1, s2);
  c1[1] = Z3_mk_bvult(ctxt, s1, d2);
  cls[0] = Z3_mk_and(ctxt, 2, c1);

  Z3_ast c2[2];
  c2[0] = Z3_mk_bvuge(ctxt, s2, s1);
  c2[1] = Z3_mk_bvult(ctxt, s2, d1);
  cls[1] = Z3_mk_and(ctxt, 2, c2);

  addAssert(Z3_mk_or(ctxt, 2, cls));

  // solve
  Z3_lbool result = Z3_solver_check(ctxt, solver);

  // restore context
  Z3_solver_pop(ctxt, solver, 1);

  return result;
}
