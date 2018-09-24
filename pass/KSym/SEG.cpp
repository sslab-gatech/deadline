#include "Project.h"

/* [disabled], to be enabeld after handling page_address
#define KSYM_CONFIG_FORCE_ASSERT
*/

// build linkage in SEOpv
void SEOpv::setHost(SENode *h) {
  host = h;

  for(SENode *v : vals){
    v->addUsr(h);
    h->addDep(v);
  }
}

// an SEOpv is ready when all its values are symbolized
bool SEOpv::ready(SymExec &sym) {
  for(SENode *v : vals) {
    SymVar *svar = v->getSymbol(sym);
    if(svar == nullptr){
      return false;
    }

    SymVal *sval = svar->getSingleVal();
    if(!sval->ready()){
      return false;
    }
  }

  return true;
}

SymVar *SENodeLeaf::getSymbol(SymExec &sym) {
  bool exist;
  SymVar *var = sym.getOrCreateVar(this, exist);

  if(!exist){
    symbolize(sym, var);
  }

  return var;
}

SymVar *SENodeVar::getSymbol(SymExec &sym) {
  bool exist;
  SymVar *var = sym.getOrCreateVar(this, exist);

  if(!exist){
    symbolize(sym, var);
  }

  return var;
}

SymVar *SENodeInst::getSymbol(SymExec &sym) {
  // TODO to be removed after symbolize for all subtypes are done
  // this also asserts on non-recursiveness between cur and preds
  for(SEOpv *i : opval) {
    if(!i->ready(sym)){
      return nullptr;
    }
  }
  // end of TODO

  bool exist;
  SymVar *var = sym.getOrCreateVar(this, exist);

  if(!exist){
    symbolize(sym, var);
  }

  return var;
}

SymVar *SENodeUnknown::getSymbol(SymExec &sym) {
#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_CASE)
  if(CallInst *ci = dyn_cast<CallInst>(val)){
    if(ci->isInlineAsm()){
      // inline asm case, do nothing
    } 
    
    else {
      Function *tar = ci->getCalledFunction();
      if(tar == nullptr){
        // indirect call case, do nothing
      } else {
        // direct call
        std::string fn = tar->getName().str();
        if(tar->isIntrinsic()){
          Helper::convertDotInName(fn);
        }

        errs() 
          << ci->getParent()->getParent()->getName() << "::"
          << fn << "\n";
      }
    }
  }
#endif

  return nullptr;
}

// generic symbolize - opv
void SEOpv0::symbolize(SymExec &sym, SymVar *var) {
  var->add(derive(sym));
}

void SEOpv1::symbolize(SymExec &sym, SymVar *var) {
  SENode *v0 = at(0);
  SymVar *r0 = v0->getSymbol(sym);

  SymVar::iterator i0 = r0->begin(), i0e = r0->end();
  for(; i0 != i0e; ++i0){
    var->add(derive(sym, *i0));
  }
}

void SEOpv2::symbolize(SymExec &sym, SymVar *var) {
  SENode *v0 = at(0);
  SymVar *r0 = v0->getSymbol(sym);

  SENode *v1 = at(1);
  SymVar *r1 = v1->getSymbol(sym);

  SymVar::iterator i0 = r0->begin(), i0e = r0->end();
  for(; i0 != i0e; ++i0){
    SymVar::iterator i1 = r1->begin(), i1e = r1->end();
    for(; i1 != i1e; ++i1){
      var->add(derive(sym, *i0, *i1));
    }
  }
}

void SEOpv3::symbolize(SymExec &sym, SymVar *var) {
  SENode *v0 = at(0);
  SymVar *r0 = v0->getSymbol(sym);

  SENode *v1 = at(1);
  SymVar *r1 = v1->getSymbol(sym);

  SENode *v2 = at(2);
  SymVar *r2 = v2->getSymbol(sym);

  SymVar::iterator i0 = r0->begin(), i0e = r0->end();
  for(; i0 != i0e; ++i0){
    SymVar::iterator i1 = r1->begin(), i1e = r1->end();
    for(; i1 != i1e; ++i1){
      SymVar::iterator i2 = r2->begin(), i2e = r2->end();
      for(; i2 != i2e; ++i2){
        var->add(derive(sym, *i0, *i1, *i2));
      }
    }
  }
}

// concrete symbolize
#define CHECK_NODE(val_type)                                          \
  assert(isa<val_type>(val));                                         \
  val_type *op = cast<val_type>(val);

#define CHECK_INST(val_type)                                          \
  assert(isa<val_type>(inst));                                        \
  val_type *op = cast<val_type>(inst);

#define GET_ORACLE                                                    \
  ModuleOracle &oracle = sym.getOracle();

#define INIT_EXPR                                                     \
  SymExpr expr;                                                       \
  SymCond cond;

#define FINI_EXPR                                                     \
  return new SymVal(expr, cond);

SymVal *SENodeCInt::derive(SymExec &sym) {
  INIT_EXPR;

  CHECK_NODE(ConstantInt);

  expr.expr = sym.createConstMachInt(op);

  FINI_EXPR;
}

SymVal *SENodeCNull::derive(SymExec &sym) {
  INIT_EXPR;

  CHECK_NODE(ConstantPointerNull);

  expr.expr = sym.createConstNull();

  FINI_EXPR;
}

SymVal *SENodeParam::derive(SymExec &sym) {
  INIT_EXPR;

  CHECK_NODE(Argument);
  GET_ORACLE;

  // very conservative check on whether this param is a pointer or value
  bool ptr = false;
  Type *ty = op->getType();

  if(ty->isPointerTy()){
    ptr = true;
  }

  else if(oracle.isReintPointerType(ty)){
    for(SENode *u : usrs){
      SENodeInst *ui = dyn_cast<SENodeInst>(u);
      assert(ui != nullptr);
      if(isa<SEOpvCast2Ptr>(ui->getSingleOpv())){
        ptr = true;
        break;
      }
    }
  }

  if(ptr){
    expr.expr = sym.createPtrExtern();
  } else {
    expr.expr = sym.createVarMachInt(ty->getIntegerBitWidth());
  }

  FINI_EXPR;
}

SymVal *SENodeGlobal::derive(SymExec &sym) {
  INIT_EXPR;

  CHECK_NODE(GlobalVariable);

  // only pointer type is allowed for global variables
  Type *ty = op->getType();
  assert(ty->isPointerTy());

  expr.expr = sym.createPtrExtern();

  FINI_EXPR;
}

SymVal *SENodeLocal::derive(SymExec &sym) {
  INIT_EXPR;

  CHECK_NODE(AllocaInst);

  // only pointer type is allowed for local variables
  Type *ty = op->getType();
  assert(ty->isPointerTy());

  expr.expr = sym.createPtrStack();

  FINI_EXPR;
}

SymVal *SEOpvCastTrunc::derive(SymExec &sym,
    SymVal *orig) {

  INIT_EXPR;

  CHECK_INST(CastInst);
  GET_ORACLE;

  expr.expr = sym.castMachIntTrunc(orig->getExpr().expr,
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

SymVal *SEOpvCastZExt::derive(SymExec &sym,
    SymVal *orig) {

  INIT_EXPR;

  CHECK_INST(CastInst);
  GET_ORACLE;

  expr.expr = sym.castMachIntZExt(orig->getExpr().expr,
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

SymVal *SEOpvCastSExt::derive(SymExec &sym,
    SymVal *orig) {

  INIT_EXPR;

  CHECK_INST(CastInst);
  GET_ORACLE;

  expr.expr = sym.castMachIntSExt(orig->getExpr().expr,
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

SymVal *SEOpvCast2Ptr::derive(SymExec &sym,
    SymVal *orig) {

  INIT_EXPR;

  CHECK_INST(CastInst);

  expr.expr = orig->getExpr().expr;

  FINI_EXPR;
}

SymVal *SEOpvCast2Int::derive(SymExec &sym,
    SymVal *orig) {

  INIT_EXPR;

  CHECK_INST(CastInst);

  expr.expr = orig->getExpr().expr;

  FINI_EXPR;
}

SymVal *SEOpvCastType::derive(SymExec &sym,
    SymVal *orig) {

  INIT_EXPR;

  CHECK_INST(CastInst);

  expr.expr = orig->getExpr().expr;

  FINI_EXPR;
}

SymVal *SEOpvCalcAdd::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp,
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvadd(sym.getContext(), lexp, rexp);

  // overflow/underflow assertions
  if(op->hasNoSignedWrap()){
    sym.addAssert(Z3_mk_bvadd_no_overflow(sym.getContext(), lexp, rexp, true));
  }

  if(op->hasNoUnsignedWrap()){
    sym.addAssert(Z3_mk_bvadd_no_overflow(sym.getContext(), lexp, rexp, false));
  }

  sym.addAssert(Z3_mk_bvadd_no_underflow(sym.getContext(), lexp, rexp));

  FINI_EXPR;
}

SymVal *SEOpvCalcSub::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp,
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvsub(sym.getContext(), lexp, rexp);

  // overflow/underflow assertions
  if(op->hasNoSignedWrap()){
    sym.addAssert(Z3_mk_bvsub_no_underflow(sym.getContext(), lexp, rexp, true));
  }

  if(op->hasNoUnsignedWrap()){
    sym.addAssert(Z3_mk_bvsub_no_underflow(sym.getContext(), lexp, rexp, false));
  }

  sym.addAssert(Z3_mk_bvsub_no_overflow(sym.getContext(), lexp, rexp));

  FINI_EXPR;
}

SymVal *SEOpvCalcMul::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvmul(sym.getContext(), lexp, rexp);

  // overflow/underflow assertions
  if(op->hasNoSignedWrap()){
    sym.addAssert(Z3_mk_bvmul_no_overflow(sym.getContext(), lexp, rexp, true));
  }

  if(op->hasNoUnsignedWrap()){
    sym.addAssert(Z3_mk_bvmul_no_overflow(sym.getContext(), lexp, rexp, false));
  }

  sym.addAssert(Z3_mk_bvmul_no_underflow(sym.getContext(), lexp, rexp));

  FINI_EXPR;
}

SymVal *SEOpvCalcUDiv::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvudiv(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcSDiv::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvsdiv(sym.getContext(), lexp, rexp);

  // overflow/underflow assertions
  sym.addAssert(Z3_mk_bvsdiv_no_overflow(sym.getContext(), lexp, rexp));

  FINI_EXPR;
}

SymVal *SEOpvCalcURem::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvurem(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcSRem::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvsrem(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcShl::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvshl(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcLShr::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvlshr(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcAShr::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvashr(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcAnd::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvand(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcOr::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvor(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCalcXor::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(BinaryOperator);

  // adjust sizes for math ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCalcOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = Z3_mk_bvxor(sym.getContext(), lexp, rexp);

  FINI_EXPR;
}

SymVal *SEOpvCmpEq::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(CmpInst);
  GET_ORACLE;

  // adjust sizes for cmp ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCmpOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = sym.castBoolToMachInt(
      Z3_mk_eq(sym.getContext(), lexp, rexp),
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

SymVal *SEOpvCmpNe::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(CmpInst);
  GET_ORACLE;

  // adjust sizes for cmp ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCmpOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  expr.expr = sym.castBoolToMachIntInversed(
      Z3_mk_eq(sym.getContext(), lexp, rexp),
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

SymVal *SEOpvCmpRel::derive(SymExec &sym,
    SymVal *lhs, SymVal *rhs) {

  INIT_EXPR;

  CHECK_INST(CmpInst);
  GET_ORACLE;

  // adjust sizes for cmp ops
  Z3_ast lexp = lhs->getExpr().expr;
  Z3_ast rexp = rhs->getExpr().expr;

  sym.prepareCmpOperation(
      op->getOperand(0)->getType(), lexp, 
      op->getOperand(1)->getType(), rexp);

  // generic function pointer
  Z3_ast (*action)(Z3_context, Z3_ast, Z3_ast);

  switch(op->getPredicate()){
    case CmpInst::ICMP_UGT:
      action = Z3_mk_bvugt;
      break;
    case CmpInst::ICMP_UGE:
      action = Z3_mk_bvuge;
      break;
    case CmpInst::ICMP_ULT:
      action = Z3_mk_bvult;
      break;
    case CmpInst::ICMP_ULE:
      action = Z3_mk_bvule;
      break;
    case CmpInst::ICMP_SGT:
      action = Z3_mk_bvsgt;
      break;
    case CmpInst::ICMP_SGE:
      action = Z3_mk_bvsge;
      break;
    case CmpInst::ICMP_SLT:
      action = Z3_mk_bvslt;
      break;
    case CmpInst::ICMP_SLE:
      action = Z3_mk_bvsle;
      break;
    default:
      DUMP.typedValue(op);
      llvm_unreachable("Unknown CMP predicate");
      break;
  }

  expr.expr = sym.castBoolToMachInt(
      action(sym.getContext(), lexp, rexp),
      oracle.getTypeWidth(op->getType()));

  FINI_EXPR;
}

static Z3_ast deriveGEPExpr(GetElementPtrInst *gep,
    SymExec &sym, SymVal *pval, queue<SymVal *> &idxs) {

  // build the offset expression
  Z3_ast expr = sym.createConstPointer(0);

  // follow GEP by index
  Value *ptr = gep->getPointerOperand();
  
  Value *idx;
  Type *cty, *nty;

  cty = ptr->getType();

  User::op_iterator i = gep->idx_begin(), ie = gep->idx_end();
  for(; i != ie; ++i){
    idx = i->get();

    // nothing to GEP on if it is integer type
    assert(!isa<IntegerType>(cty));

    if(isa<PointerType>(cty)){
      nty = cty->getPointerElementType();

      Z3_ast tys = sym.createConstTypeSize(nty);

      Z3_ast tid;
      if(isa<ConstantInt>(idx)){
        tid = sym.createConstPointerFromInt(cast<ConstantInt>(idx));
      } else {
        tid = idxs.front()->getExpr().expr;
        idxs.pop();
      }

      Z3_ast off = Z3_mk_bvmul(sym.getContext(), tys, tid);
      expr = Z3_mk_bvadd(sym.getContext(), expr, off);
    }

    else if(isa<StructType>(cty)){
      assert(isa<ConstantInt>(idx));
      unsigned fid = cast<ConstantInt>(idx)->getZExtValue();
      nty = cty->getStructElementType(fid);

      Z3_ast off = sym.createConstTypeOffset(cty, fid);
      expr = Z3_mk_bvadd(sym.getContext(), expr, off);
    }

    else if(isa<ArrayType>(cty)){
      nty = cty->getArrayElementType();

      Z3_ast tys = sym.createConstTypeSize(nty);

      Z3_ast tid;
      if(isa<ConstantInt>(idx)){
        tid = sym.createConstPointerFromInt(cast<ConstantInt>(idx));
      } else {
        tid = idxs.front()->getExpr().expr;
        idxs.pop();
      }

      Z3_ast off = Z3_mk_bvmul(sym.getContext(), tys, tid);
      expr = Z3_mk_bvadd(sym.getContext(), expr, off);
    }

    else {
      DUMP.typedValue(gep);
      llvm_unreachable("Unhandled GEP type"); 
    }

    cty = nty;
  }

  // ensure we unrolled correctly
  assert(cty == gep->getResultElementType());
  assert(idxs.empty());

  // get full expr
  return Z3_mk_bvadd(sym.getContext(), pval->getExpr().expr, expr);
}

SymVal *SEOpvGEPIdx0::derive(SymExec &sym,
    SymVal *ptr) {

  INIT_EXPR;

  CHECK_INST(GetElementPtrInst);

  queue<SymVal *> idxs;

  expr.expr = deriveGEPExpr(op, sym, ptr, idxs);

  FINI_EXPR;
}

SymVal *SEOpvGEPIdx1::derive(SymExec &sym,
    SymVal *ptr, SymVal *idx0) {

  INIT_EXPR;

  CHECK_INST(GetElementPtrInst);

  queue<SymVal *> idxs;
  idxs.push(idx0);

  expr.expr = deriveGEPExpr(op, sym, ptr, idxs);

  FINI_EXPR;
}

SymVal *SEOpvGEPIdx2::derive(SymExec &sym,
    SymVal *ptr, SymVal *idx0, SymVal *idx1) {

  INIT_EXPR;

  CHECK_INST(GetElementPtrInst);

  queue<SymVal *> idxs;
  idxs.push(idx0);
  idxs.push(idx1);

  expr.expr = deriveGEPExpr(op, sym, ptr, idxs);

  FINI_EXPR;
}

SymVal *SEOpvPhi::derive(SymExec &sym,
    SymVal *tran) {

  INIT_EXPR;

  CHECK_INST(PHINode);

  expr.expr = tran->getExpr().expr;

  FINI_EXPR;
}

SymVal *SEOpvSelect::derive(SymExec &sym,
    SymVal *cval, SymVal *tval, SymVal *fval) {

  INIT_EXPR;

  CHECK_INST(SelectInst);

  expr.expr = Z3_mk_ite(sym.getContext(),
      sym.castMachIntToBool(cval->getExpr().expr), 
      tval->getExpr().expr, fval->getExpr().expr);

  FINI_EXPR;
}

SymVal *SEOpvBranch::derive(SymExec &sym,
    SymVal *cval) {

  INIT_EXPR;

  CHECK_INST(BranchInst);

  int c = host->getGraph()->getCond(host);
  if(c == 0){
    // true value taken
    sym.addAssert(sym.castMachIntToBool(cval->getExpr().expr));
  } else if(c == 1){
    // false value taken
    sym.addAssert(sym.castMachIntToBoolInversed(cval->getExpr().expr));
  } else {
    // no branch is taken, do nothing
  }

  expr.expr = sym.createSpecialNone();

  FINI_EXPR;
}

SymVal *SEOpvLoad::derive(SymExec &sym,
    SymVal *ptr) {

  INIT_EXPR;

  CHECK_INST(LoadInst);
  GET_ORACLE;

  expr.expr = sym.loadMemory(ptr->getExpr().expr, 
      oracle.getTypeSize(op->getType()));

  FINI_EXPR;
}

SymVal *SEOpvStore::derive(SymExec &sym,
    SymVal *ptr, SymVal *vop) {

  INIT_EXPR;

  CHECK_INST(StoreInst);

  sym.storeMemory(ptr->getExpr().expr, vop->getExpr().expr);

  expr.expr = sym.createSpecialNone();

  FINI_EXPR;
}

SymVal *SEOpvExtVal::derive(SymExec &sym,
    SymVal *ptr) {

  INIT_EXPR;

  CHECK_INST(ExtractValueInst);

  expr.expr = ptr->getExpr().expr;

  FINI_EXPR;
}

// locators
bool SEGraph::locateValue(int cur, Value *val, int &seq) {
  // none-instructions are always located at location -1
  if(!isa<Instruction>(val)){
    seq = -1;
    return true;
  }

  // instructions can only be located from instructions
  assert(cur >= 0);

  for(seq = cur; seq >= 0; seq--){
    if(trace.at(seq) == val){
      return true;
    }
  }

  return false;
}

// node getter
SENode *SEGraph::getNode(int seq, Value *val) {
  auto k = make_pair(seq, val);

  // first check whether the node exists
  auto i = nodes.find(k);
  if(i != nodes.end()){
    return i->second;
  }

  // then make sure the value exists at given location
  if(seq < 0){
    assert(!isa<Instruction>(val));
  } else {
    assert(trace.at(seq) == val);
  }

  // finally build the node
  SENode *res = buildNode(seq, val);
  nodes.insert(make_pair(k, res));
  return res;
}

SENode *SEGraph::getNodeOrBuild(int cur, Value *val) {
  // find the seq for the val
  int seq;
  assert(locateValue(cur, val, seq));
  assert(seq <= cur);

  return getNode(seq, val);
}

SENode *SEGraph::getNodeOrNull(int seq, Value *val) {
  auto k = make_pair(seq, val);
  auto i = nodes.find(k);
  if(i == nodes.end()){
    return nullptr;
  } else {
    return i->second;
  }
}

SENode *SEGraph::getNodeOrFail(int cur, Value *val) {
  // find the seq for the val
  int seq;
  assert(locateValue(cur, val, seq));
  assert(seq <= cur);

  // get the node
  SENode *res = getNodeOrNull(seq, val);
  assert(res != nullptr);
  return res;
}

SENode *SEGraph::getNodeProbe(Value *val) {
  if(!isa<Instruction>(val)){
    return getNodeOrNull(-1, val);
  }

  SENode *node;
  for(int i = trace.size() - 1; i >= 0; i--){
    node = getNodeOrNull(i, val);
    if(node != nullptr){
      return node;
    }
  }

  return nullptr;
}


// node builder
#define INIT_TYPE(val_type)                                         \
  if(isa<val_type>(val)) {                                          \
    val_type *op = cast<val_type>(val);

#define FINI_TYPE                                                   \
  }

#define INIT_NODE(se_type)                                          \
  SENode##se_type *node = new SENode##se_type(seq, op, this);       \
  addNode(node);

#define FINI_NODE                                                   \
  return node;

#define INIT_TYPE_NODE(val_type, se_type)                           \
  INIT_TYPE(val_type)                                               \
  INIT_NODE(se_type)

#define FINI_TYPE_NODE                                              \
  FINI_NODE                                                         \
  FINI_TYPE

#define IGNORE_NODE                                                 \
  SENodeUnknown *node = new SENodeUnknown(seq, op, this);           \
  addNode(node);                                                    \
  return node;

#define IGNORE_TYPE(val_type)                                       \
  INIT_TYPE(val_type)                                               \
  IGNORE_NODE                                                       \
  FINI_TYPE

#define UNHANDLED(msg)                                              \
  DUMP.typedValue(val);                                             \
  llvm_unreachable("Unhandled: " msg);                              \
  return nullptr;

SENode *SEGraph::buildNode(int seq, Value *val) {
  // variables
  INIT_TYPE_NODE(Argument, Param) {} FINI_TYPE_NODE;

  INIT_TYPE_NODE(GlobalVariable, Global) {} FINI_TYPE_NODE;

  INIT_TYPE_NODE(AllocaInst, Local) {} FINI_TYPE_NODE;

  IGNORE_TYPE(UndefValue);

  // constants
  INIT_TYPE_NODE(ConstantInt, CInt) {} FINI_TYPE_NODE;

  INIT_TYPE_NODE(ConstantPointerNull, CNull) {} FINI_TYPE_NODE;

  // instructions
  INIT_TYPE_NODE(CastInst, Cast) {
    SENode *orig = getNodeOrBuild(seq, op->getOperand(0));

    switch(op->getOpcode()){
      case Instruction::Trunc:
        node->addOpv(new SEOpvCastTrunc(seq, op, orig));
        break;
      case Instruction::ZExt:
        node->addOpv(new SEOpvCastZExt(seq, op, orig));
        break;
      case Instruction::SExt:
        node->addOpv(new SEOpvCastSExt(seq, op, orig));
        break;
      case Instruction::PtrToInt:
        node->addOpv(new SEOpvCast2Int(seq, op, orig));
        break;
      case Instruction::IntToPtr:
        node->addOpv(new SEOpvCast2Ptr(seq, op, orig));
        break;
      case Instruction::BitCast:
        node->addOpv(new SEOpvCastType(seq, op, orig));
        break;
      default:
        DUMP.typedValue(val);
        llvm_unreachable("Unhandled cast type");
        break;
    }

  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(BinaryOperator, Calc) {
    SENode *lhs = getNodeOrBuild(seq, op->getOperand(0));
    SENode *rhs = getNodeOrBuild(seq, op->getOperand(1));

    switch(op->getOpcode()){
      case Instruction::Add:
        node->addOpv(new SEOpvCalcAdd(seq, op, lhs, rhs));
        break;
      case Instruction::Sub:
        node->addOpv(new SEOpvCalcSub(seq, op, lhs, rhs));
        break;
      case Instruction::Mul:
        node->addOpv(new SEOpvCalcMul(seq, op, lhs, rhs));
        break;
      case Instruction::UDiv:
        node->addOpv(new SEOpvCalcUDiv(seq, op, lhs, rhs));
        break;
      case Instruction::SDiv:
        node->addOpv(new SEOpvCalcSDiv(seq, op, lhs, rhs));
        break;
      case Instruction::URem:
        node->addOpv(new SEOpvCalcURem(seq, op, lhs, rhs));
        break;
      case Instruction::SRem:
        node->addOpv(new SEOpvCalcSRem(seq, op, lhs, rhs));
        break;
      case Instruction::Shl:
        node->addOpv(new SEOpvCalcShl(seq, op, lhs, rhs));
        break;
      case Instruction::LShr:
        node->addOpv(new SEOpvCalcLShr(seq, op, lhs, rhs));
        break;
      case Instruction::AShr:
        node->addOpv(new SEOpvCalcAShr(seq, op, lhs, rhs));
        break;
      case Instruction::And:
        node->addOpv(new SEOpvCalcAnd(seq, op, lhs, rhs));
        break;
      case Instruction::Or:
        node->addOpv(new SEOpvCalcOr(seq, op, lhs, rhs));
        break;
      case Instruction::Xor:
        node->addOpv(new SEOpvCalcXor(seq, op, lhs, rhs));
        break;
      default:
        DUMP.typedValue(val);
        llvm_unreachable("Unhandled binary operator");
        break;
    }
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(CmpInst, Cmp) {
    assert(op->getOpcode() == Instruction::ICmp);

    SENode *lhs = getNodeOrBuild(seq, op->getOperand(0));
    SENode *rhs = getNodeOrBuild(seq, op->getOperand(1));

    switch(op->getPredicate()){
      case CmpInst::ICMP_EQ:
        node->addOpv(new SEOpvCmpEq(seq, op, lhs, rhs));
        break;

      case CmpInst::ICMP_NE:
        node->addOpv(new SEOpvCmpNe(seq, op, lhs, rhs));
        break;

      default:
        node->addOpv(new SEOpvCmpRel(seq, op, lhs, rhs));
        break;
    }
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(GetElementPtrInst, GEP) {
    vector<Value *> vars;
    SEGUtil::decompose(op, vars);
    // NOTE there is no hard limit on this num, it is just that it fits
    // well with GEPIdx0/1/2
    assert(vars.size() <= 2);

    SENode *ptr = getNodeOrBuild(seq, op->getPointerOperand());
    switch(vars.size()) {
      case 0:
        node->addOpv(new SEOpvGEPIdx0(seq, op, ptr));
        break;
      case 1:
        {
          SENode *idx0 = getNodeOrBuild(seq, vars[0]);
          node->addOpv(new SEOpvGEPIdx1(seq, op, ptr, idx0));
        }
        break;
      case 2:
        {
          SENode *idx0 = getNodeOrBuild(seq, vars[0]);
          SENode *idx1 = getNodeOrBuild(seq, vars[1]);
          node->addOpv(new SEOpvGEPIdx2(seq, op, ptr, idx0, idx1));
        }
        break;
      default:
        llvm_unreachable("Too many unknown variables in GEP index");
        break;
    }
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(PHINode, Phi) {
    Value *opv = SEGUtil::backtrace(seq, op, trace, so);
    SENode *tran = getNodeOrBuild(seq, opv);
    node->addOpv(new SEOpvPhi(seq, op, tran));
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(SelectInst, Select) {
    SENode *cval = getNodeOrBuild(seq, op->getCondition());
    SENode *tval = getNodeOrBuild(seq, op->getTrueValue());
    SENode *fval = getNodeOrBuild(seq, op->getFalseValue());
    node->addOpv(new SEOpvSelect(seq, op, cval, tval, fval));
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(BranchInst, Branch) {
    SENode *cval = getNodeOrBuild(seq, op->getCondition());
    node->addOpv(new SEOpvBranch(seq, op, cval));
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(LoadInst, Load) {
    SENode *ptr = getNodeOrBuild(seq, op->getPointerOperand());
    node->addOpv(new SEOpvLoad(seq, op, ptr));
  } FINI_TYPE_NODE;

  INIT_TYPE_NODE(StoreInst, Store) {
    SENode *ptr = getNodeOrBuild(seq, op->getPointerOperand());
    SENode *vop = getNodeOrBuild(seq, op->getValueOperand());
    node->addOpv(new SEOpvStore(seq, op, ptr, vop));
  } FINI_TYPE_NODE;

  INIT_TYPE(CallInst) {
    // inline asm
    if(op->isInlineAsm()){
      InlineAsm *bin = dyn_cast<InlineAsm>(op->getCalledValue());
      assert(bin != nullptr);
      std::string fn = bin->getAsmString();

#define ASMCALL_NODE
#include "Asmcall.def"
#undef ASMCALL_NODE

      // TODO other inline asm
      IGNORE_NODE;
    }

    // lib calls
    Function *tar = op->getCalledFunction();
    if(tar == nullptr){
      // TODO indirect call
      IGNORE_NODE;
    }

    std::string fn = tar->getName().str();
    if(tar->isIntrinsic()){
      Helper::convertDotInName(fn);
    }

#define LIBCALL_NODE
#include "Libcall.def"
#undef LIBCALL_NODE

    // TODO other lib calls
    IGNORE_NODE;
  } FINI_TYPE;

  INIT_TYPE_NODE(ExtractValueInst, ExtVal) {
    SENode *ptr = getNodeOrBuild(seq, op->getAggregateOperand());
    node->addOpv(new SEOpvExtVal(seq, op, ptr));
  } FINI_TYPE_NODE;

  // TODO, handle ExtractElementInst and InsertElemInst
  IGNORE_TYPE(ExtractElementInst);
  IGNORE_TYPE(InsertElementInst);
  IGNORE_TYPE(Function);

  // should have enumerated all cases
  UNHANDLED("Unknown value type for node building");
}

// SEGraph construction
static inline void collectDeps(SENode *node, set<SENode *> &deps) {
  if(deps.find(node) != deps.end()){
    return;
  }

  deps.insert(node);

  SENode::linkIter di = node->depBegin(), de = node->depEnd();
  for(; di != de; ++di){
    collectDeps(*di, deps);
  }
}

void SEGraph::followTrace() {
  int seq = 0;

  for(Instruction *inst : trace){
    // steped on a condition or a connection
    if(isa<TerminatorInst>(inst)){
      BranchInst *branch = dyn_cast<BranchInst>(inst);
      assert(branch != nullptr);
      
      if(branch->isConditional()){
        SENode *brch = getNodeOrBuild(seq, branch);

        SliceBlock *host = so.getSliceHost(branch);
        assert(host != nullptr);

        SliceBlock *next = so.getSliceHost(trace.at(seq + 1));
        assert(next != nullptr);

        BasicBlock *tval = branch->getSuccessor(0);
        BasicBlock *fval = branch->getSuccessor(1);

        if(host->inSTab(next, tval) && host->inSTab(next, fval)){
          // this branch is irrelevant
        }

        else if(host->inSTab(next, tval)){
          conds.insert(make_pair(brch, 0));
        }

        else {
          conds.insert(make_pair(brch, 1));
        }
      }
    }

    // normal instrucitons
    else {
      getNodeOrBuild(seq, inst);
    }

    // go to the next inst in list
    seq++;
  }
}

void SEGraph::trimGraph() {
  set<SENode *> deps;

  // collect init set
  SENode *term = getNode(trace.size() - 1, trace.back());
  assert(term != nullptr);
  collectDeps(term, deps);

  for(auto &i : conds){
    SENode *cond = i.first;
    collectDeps(cond, deps);
  }

  // expand the set
  bool changed = true;
  while(changed) {
    changed = false;

    for(auto &i : nodes){
      SENode *node = i.second;
      if(deps.find(node) != deps.end()){
        continue;
      }

      SENode::linkIter di = node->depBegin(), de = node->depEnd();
      for(; di != de; ++di){
        if(deps.find(*di) != deps.end()){
          collectDeps(node, deps);
          changed = true;
          break;
        }
      }

      if(changed){
        break;
      }
    }
  }

  // delete non-related nodes
  set<SENode *> dels;
  for(auto &i : nodes){
    SENode *node = i.second;
    if(deps.find(node) == deps.end()){
      dels.insert(node);
    }
  }

  for(SENode *node : dels){
    SENode::linkIter di = node->depBegin(), de = node->depEnd();
    for(; di != de; ++di){
      (*di)->delUsr(node);
    }

    SENode::linkIter ui = node->usrBegin(), ue = node->usrEnd();
    for(; ui != ue; ++ui){
      (*ui)->delDep(node);
    }

    nodes.erase(make_pair(node->getSeq(), node->getVal()));
    delete node;
  }
}

void SEGraph::filterTrace(iseq &filt) {
  int len = trace.size();
  for(int c = 0; c < len; c++){
    Instruction *i = trace.at(c);

    // check if the stmt is in SEG
    SENode *node = getNodeOrNull(c, i);
    if(node == nullptr){
      continue;
    }

    // replay the node
    filt.push_back(i);
  }
}
