#include "Project.h"

// the strategy to collect conditions 
// [disabled] #define KSYM_CONFIG_SLICE_COMPLETE

// Pre-procesing
static void collectInsts(set<BasicBlock *> &reach, set<Instruction *> &insts) {
  for(BasicBlock *bb : reach){
    for(Instruction &i : *bb){
      insts.insert(&i);
    }
  }
}

// Backtracing
bool Slice::btrace(Value *v) {
  // test if we have backtraced this value
  if(backs.find(v) != backs.end()){
    return false;
  }

  // check if v is a constant
  if(isa<ConstantData>(v) || isa<ConstantAggregate>(v)){
    return false;
  }

  // mark that we have backtraced this value
  backs.insert(v);

  // check if v is a root
  if(isa<Argument>(v) || isa<GlobalObject>(v) || isa<AllocaInst>(v)){
    return roots.insert(v).second;
  }

  // further decompose v
  bool res = false;

  if(ConstantExpr *i_cexp = dyn_cast<ConstantExpr>(v)){
    // treat constant expr as regular instructions
    User::op_iterator oi = i_cexp->op_begin(), oe = i_cexp->op_end();
    for(; oi != oe; ++oi){
      if(btrace(*oi)){
        res = true;
      }
    }
  }

  else if(BranchInst *i_brch = dyn_cast<BranchInst>(v)){
    // only trace the condition value
    if(i_brch->isConditional()){
      if(btrace(i_brch->getCondition())){
        res = true;
      }
    }
  }

  else if(CallInst *i_call = dyn_cast<CallInst>(v)){
    // NOTE: CallInst is a black hole which might suck up everything,
    // so take caution not to trace unwanted conditions
    if(i_call->isInlineAsm()){
      InlineAsm *bin = dyn_cast<InlineAsm>(i_call->getCalledValue());
      assert(bin != nullptr);
      std::string fn = bin->getAsmString();

      if(fn.empty()){
        llvm_unreachable("Asm string cannot be empty");
      }

#define ASMCALL_BACK
#include "Asmcall.def"
#undef ASMCALL_BACK

      else {
        // default to ignore the rest
        /*
        errs() 
          << i_call->getParent()->getParent()->getName() << "::" 
          << fn << "\n";
        */
        // TODO handle the rest of the calls
      }
    }

    else {
      Function *tar = i_call->getCalledFunction();
      if(tar == nullptr){
        // TODO handle indirect call
      }

      else {
        // direct call, selectively trace the arguments
        std::string fn = tar->getName().str();
        if(tar->isIntrinsic()){
          Helper::convertDotInName(fn);
        }

        if(fn.empty()){
          llvm_unreachable("Function name cannot be empty");
        }

#define LIBCALL_BACK
#include "Libcall.def"
#undef LIBCALL_BACK

        else {
          // default to ignore the rest
          /*
          errs() 
            << i_call->getParent()->getParent()->getName() << "::" 
            << fn << "\n";
          */
          // TODO handle the rest of the calls
        }
      }
    }
  }

  else if(isa<ExtractElementInst>(v) || isa<InsertElementInst>(v)){
    // handle these rare cases in a generic way
    Instruction *inst = cast<Instruction>(v);

    User::op_iterator oi = inst->op_begin(), oe = inst->op_end();
    for(; oi != oe; ++oi){
      if(btrace(*oi)){
        res = true;
      }
    }
  }

  else if(BinaryOperator *i_bin = dyn_cast<BinaryOperator>(v)) {
    if(btrace(i_bin->getOperand(0))){
      res = true;
    }

    if(btrace(i_bin->getOperand(1))){
      res = true;
    }
  }

  else if(CastInst *i_cast = dyn_cast<CastInst>(v)){
    if(btrace(i_cast->getOperand(0))){
      res = true;
    }
  }

  else if(CmpInst *i_cmp = dyn_cast<CmpInst>(v)){
    if(btrace(i_cmp->getOperand(0))){
      res = true;
    }

    if(btrace(i_cmp->getOperand(1))){
      res = true;
    }
  }

  else if(ExtractValueInst *i_ext = dyn_cast<ExtractValueInst>(v)){
    if(btrace(i_ext->getAggregateOperand())){
      res = true;
    }
  }

  else if(GetElementPtrInst *i_gep = dyn_cast<GetElementPtrInst>(v)){
    if(btrace(i_gep->getPointerOperand())){
      res = true;
    }

    User::op_iterator oi = i_gep->idx_begin(), oe = i_gep->idx_end();
    for(; oi != oe; ++oi){
      if(btrace(*oi)){
        res = true;
      }
    }
  }

  else if(LoadInst *i_load = dyn_cast<LoadInst>(v)){
    if(btrace(i_load->getPointerOperand())){
      res = true;
    }
  }

  else if(PHINode *i_phi = dyn_cast<PHINode>(v)){
    unsigned num = i_phi->getNumIncomingValues();
    for(int i = 0; i < num; i++){
      if(btrace(i_phi->getIncomingValue(i))){
        res = true;
      }

      TerminatorInst *pt = i_phi->getIncomingBlock(i)->getTerminator();
      assert(isa<BranchInst>(pt));

      if(btrace(pt)){
        res = true;
      }
    }
  }

  else if(SelectInst *i_sel = dyn_cast<SelectInst>(v)){
    if(btrace(i_sel->getTrueValue())){
      res = true;
    }

    if(btrace(i_sel->getFalseValue())){
      res = true;
    }

    if(btrace(i_sel->getCondition())){
      res = true;
    }
  }

  else if(StoreInst *i_store = dyn_cast<StoreInst>(v)){
    if(btrace(i_store->getValueOperand())){
      res = true;
    }

    if(btrace(i_store->getPointerOperand())){
      res = true;
    }
  }

  else {
    // should have enumerated all
    DUMP.typedValue(v);
    llvm_unreachable("Unknown value type to btrace");
  }

  return res;
}

void Slice::follow(Value *v, set<Value *> &taints) {
  Value::user_iterator i = v->user_begin(), ie = v->user_end();
  for(; i != ie; ++i){
    User *u = *i;

    // skip btraced values
    if(backs.find(u) != backs.end()){
      continue;
    }

		// do not follow cyclic dependency (e.g., phi)
		if(taints.find(u) != taints.end()){
			continue;
		}

    if(ConstantExpr *cexp = dyn_cast<ConstantExpr>(u)){
      taints.insert(cexp);
      follow(cexp, taints);
    }

    else if(isa<ConstantAggregate>(u) || isa<GlobalObject>(u)){
      // simply ignore them
      continue;
    }

    else if(Instruction *inst = dyn_cast<Instruction>(u)){
      if(scope.find(inst) == scope.end()){
        continue;
      }

      taints.insert(inst);
      follow(inst, taints);
    }

    else {
      DUMP.typedValue(u);
      llvm_unreachable("Unknown value type to follow");
    }
  }
}

SliceBlock *Slice::apply(BasicBlock *bb) {
  // check if block is in the reachable set
  if(reach.find(bb) == reach.end()){
    return nullptr;
  }

  // construct block
  SliceBlock *slice = new SliceBlock(bb);

  for(Instruction &inst : *bb){
    if(backs.find(&inst) != backs.end()){
      slice->addInst(&inst);
    }
  }

  return slice;
}

SliceBlock *Slice::joint(BasicBlock *bb, list<BasicBlock *> &hists,
    map<BasicBlock *, SliceBlock *> &dedup, set<Value *> &taints) {

  // if block is not in the reachable set, return null
  auto i = cache.find(bb);
  if(i == cache.end()){
    return nullptr;
  }

  // if the block has been visited
  auto j = dedup.find(bb);
  if(j != dedup.end()){
    return j->second;
  }

  // if the visit history forms a loop, follow trace and return the nearest one
  auto x = std::find(hists.begin(), hists.end(), bb);
  if(x != hists.end()){
    for(; x != hists.end(); x++){
      auto y = dedup.find(*x);
      if(y != dedup.end()){
        return y->second;
      }
    }

    return nullptr;
  }

  // this is a new block
  hists.push_back(bb);
  SliceBlock *slice = i->second;

  // collect and link succs
  TerminatorInst *term = bb->getTerminator();
  unsigned num = term->getNumSuccessors();

  if(num == 0){
    // reached a return block
    assert(slice->numInsts() != 0);

    // mark it as visited
    dedup.insert(make_pair(bb, slice));
    suits.insert(slice);

    // return
    hists.pop_back();
    return slice;
  }

  if(num == 1){
    // unconditional branch
    BasicBlock *succ = term->getSuccessor(0);
    SliceBlock *next;

    if(slice->numInsts() != 0){
      // mark it as visited
      dedup.insert(make_pair(bb, slice));
      suits.insert(slice);

      // setup links
      joint(succ, hists, dedup, taints);

      // return
      hists.pop_back();
      return slice;
    }

    else {
      // follow links
      next = joint(succ, hists, dedup, taints);
      dedup.insert(make_pair(bb, next));
      
      // return
      hists.pop_back();
      return next;
    }
  }

  if(num == 2){
    // conditional branch
    BasicBlock *tval = term->getSuccessor(0), *fval = term->getSuccessor(1);
    SliceBlock *tnxt, *fnxt;

    if(slice->numInsts() != 0){
      // mark it as visited
      dedup.insert(make_pair(bb, slice));
      suits.insert(slice);

      // setup links
      tnxt = joint(tval, hists, dedup, taints);
      fnxt = joint(fval, hists, dedup, taints);

      if(tnxt == fnxt){
        // this condition is irrelevant, so do nothing
      }

      else {
#ifdef KSYM_CONFIG_SLICE_COMPLETE
        // this condition is relevant
        // (make it a condition as long as tnxt != fnxt)
        if(backs.find(term) == backs.end()){
          taints.insert(term);
        }
#else
        // this condition is relevant
        // (make it a condition when tnxt != fnxt AND tnxt/fnxt != null)
        if(tnxt != nullptr && fnxt != nullptr 
            && backs.find(term) == backs.end()){
          taints.insert(term);
        }
#endif
      }
      // return
      hists.pop_back();
      return slice;
    }

    else {
      // follow links
      tnxt = joint(tval, hists, dedup, taints);
      fnxt = joint(fval, hists, dedup, taints);

      if(tnxt == fnxt){
        // this condition is irrelevant
        dedup.insert(make_pair(bb, tnxt));

        // return
        hists.pop_back();
        return tnxt;
      }

      else {
#ifdef KSYM_CONFIG_SLICE_COMPLETE
        // this condition is relevant
        // (make it a condition as long as tnxt != fnxt)
        if(backs.find(term) == backs.end()){
          taints.insert(term);
        }

        // mark it as visited
        dedup.insert(make_pair(bb, slice));
        suits.insert(slice);

        // return
        hists.pop_back();
        return slice;
#else 
        if(tnxt == nullptr){
          dedup.insert(make_pair(bb, fnxt));

          // return
          hists.pop_back();
          return fnxt;
        }

        if(fnxt == nullptr){
          dedup.insert(make_pair(bb, tnxt));

          // return
          hists.pop_back();
          return tnxt;
        }

        // this condition is relevant
        // (make it a condition when tnxt != fnxt AND tnxt/fnxt != null)
        if(backs.find(term) == backs.end()){
          taints.insert(term);
        }

        // mark it as visited
        dedup.insert(make_pair(bb, slice));
        suits.insert(slice);

        // return
        hists.pop_back();
        return slice;
#endif
      }
    }
  }
}

void Slice::expand(SliceBlock *item) {
  BasicBlock *bb = item->getBlock();

  // init vars
  BasicBlock *base, *cur, *nxt;
  SliceBlock *slice;

  set<BasicBlock *> his;
  queue<BasicBlock *> que;

  // expand preds
  pred_iterator pi = pred_begin(bb), pe = pred_end(bb);
  for(; pi != pe; ++pi){
    base = *pi;
    his.clear();

    his.insert(base);
    que.push(base);

    while(!que.empty()){
      cur = que.front();
      que.pop();

      if(reach.find(cur) == reach.end()){
        continue;
      }

      slice = cache[cur];
      if(suits.find(slice) != suits.end()){
        slice->addSucc(item);
        item->addPred(slice);
        item->addPTabEntry(slice, base);
      } else {
        pred_iterator xi = pred_begin(cur), xe = pred_end(cur);
        for(; xi != xe; ++xi){
          nxt = *xi;
          if(his.find(nxt) == his.end()){
            his.insert(nxt);
            que.push(nxt);
          }
        }
      }
    }
  }

  // expand succs
  succ_iterator si = succ_begin(bb), se = succ_end(bb);
  for(; si != se; ++si){
    base = *si;
    his.clear();

    his.insert(base);
    que.push(base);

    while(!que.empty()){
      cur = que.front();
      que.pop();

      if(reach.find(cur) == reach.end()){
        continue;
      }

      slice = cache[cur];
      if(suits.find(slice) != suits.end()){
        slice->addPred(item);
        item->addSucc(slice);
        item->addSTabEntry(slice, base);
      } else {
        succ_iterator xi = succ_begin(cur), xe = succ_end(cur);
        for(; xi != xe; ++xi){
          nxt = *xi;
          if(his.find(nxt) == his.end()){
            his.insert(nxt);
            que.push(nxt);
          }
        }
      }
    }
  }
}

// Slice construction
Slice::Slice(set<BasicBlock *> &pred, Instruction *inst) : reach(pred) {
  // pre-process
  collectInsts(reach, scope);
  BasicBlock *head = &(inst->getParent()->getParent()->getEntryBlock());

  // collect initial taint set
  set<Value *> taints;
  taints.insert(inst);

  // init data structs
  list<BasicBlock *> hists;
  list<SliceBlock *> paths;
  map<BasicBlock *, SliceBlock *> dedup;

  // recursively expand
  while(!taints.empty()){
    // backward and forward trace
    do {
      for(Value *v : taints){
        btrace(v);
      }
      
      taints.clear();

      for(Value *v : roots){
        follow(v, taints);
      }
    } while(!taints.empty());

    // collect conditions
    for(auto const &i : cache){
      delete i.second;
    }

    cache.clear();
    suits.clear();

    for(BasicBlock *bb : reach){
      cache.insert(make_pair(bb, apply(bb)));
    }

    dedup.clear();
    hists.clear();
    entry = joint(head, hists, dedup, taints);

    assert(hists.empty());
    assert(entry != nullptr && suits.find(entry) != suits.end());
  }

  basis = cache[inst->getParent()];
  assert(basis != nullptr && suits.find(basis) != suits.end());

  // link BasicBlock and SliceBlock
  for(SliceBlock *item : suits){
    expand(item);
  }

  // verify the integrity of the slice
  verify();

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_STAT)
  float count = 1.0;
  for(Value *v : backs){
    if(isa<Instruction>(v)){
      count++;
    }
  }

  errs() 
    << "Sliced: " 
    << int(count / scope.size() * 100) << "%" 
    << "\n";
#endif
}

// Slice verification
void Slice::verify() {
  for(SliceBlock *item : suits){
    // verify linkage between SliceBlocks
    SliceBlock::linkIter pi = item->predBegin(), pe = item->predEnd();
    for(; pi != pe; ++pi){
      assert((*pi)->hasSucc(item));
      assert(item->inPTab(*pi));
    }

    SliceBlock::linkIter si = item->succBegin(), se = item->succEnd();
    for(; si != se; ++si){
      assert((*si)->hasPred(item));
      assert(item->inSTab(*si));
    }

    // verify linkage between SliceBlock and BasicBlock;
    BasicBlock *bb = item->getBlock();

    pred_iterator bpi = pred_begin(bb), bpe = pred_end(bb);
    for(; bpi != bpe; ++bpi){
      if(!item->inPTab(*bpi)){
        assert(
            reach.find(*bpi) == reach.end() || 
            suits.find(cache[*bpi]) == suits.end()
            );
      }
    }

    succ_iterator bsi = succ_begin(bb), bse = succ_end(bb);
    for(; bsi != bse; ++bsi){
      if(!item->inSTab(*bsi)){
        assert(
            reach.find(*bsi) == reach.end() ||
            suits.find(cache[*bsi]) == suits.end()
            );
      }
    }
  }
}

LLVMSlice::LLVMSlice(Slice *slice) {
  SliceBlock *ptr;
  LLVMSliceBlock *cur;
    
  SliceBlock *entry = slice->getEntry();
  SliceBlock *basis = slice->getBasis();

  // ensure that the entry block is at the front
  cur = new LLVMSliceBlock(this, entry);
  blocks.push_back(cur);
  lookup.insert(make_pair(entry, cur));

  Slice::iterator i = slice->begin(), ie = slice->end();
  for(; i != ie; ++i){
    ptr = *i;
    if(ptr == entry || ptr == basis){
      continue;
    }

    cur = new LLVMSliceBlock(this, ptr);
    blocks.push_back(cur);
    lookup.insert(make_pair(ptr, cur));
  }

  // ensure that the basis block is at the back
  if(basis != entry){
    cur = new LLVMSliceBlock(this, basis);
    blocks.push_back(cur);
    lookup.insert(make_pair(basis, cur));
  }

  // build links
  i = slice->begin(), ie = slice->end();
  for(; i != ie; ++i){
    ptr = *i;
    cur = lookup[ptr];

    SliceBlock::linkIter pi = ptr->predBegin(), pe = ptr->predEnd();
    for(; pi != pe; ++pi){
      cur->addPred(lookup[*pi]);
    }

    SliceBlock::linkIter si = ptr->succBegin(), se = ptr->succEnd();
    for(; si != se; ++si){
      cur->addSucc(lookup[*si]);
    }
  }
}
