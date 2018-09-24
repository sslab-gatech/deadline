#include "Project.h"
#include "PA.h"

// utils 
static inline Value *getCallArgOrRet(CallInst *ci, int idx) {
  return idx >= 0 ? ci->getArgOperand(idx) : ci;
}

// collection
void FuncHandle::collectFetch() {
  for(BasicBlock &bb : func){
    for(Instruction &i : bb){
      if(CallInst *ci = dyn_cast<CallInst>(&i)){
        const FetchDef *def;

        if(ci->isInlineAsm()){
          InlineAsm *target = cast<InlineAsm>(ci->getCalledValue());
          def = Fetch::findDefMatch(target->getAsmString());
        } else {
          Function *target = ci->getCalledFunction();
          if(target == nullptr){
            // TODO handle indirect call
            def = nullptr;
          } else {
            def = Fetch::findDefMatch(target->getName().str());
          }
        }

        if(def != nullptr){
          Fetch *fetch = new Fetch(ci,
              getCallArgOrRet(ci, def->src),
              getCallArgOrRet(ci, def->len),
              getCallArgOrRet(ci, def->dst));

          fts.insert(make_pair(ci, fetch));
        }
      }
    }
  }
}

Fetch *FuncHandle::getFetchFromInst(Instruction *inst) {
  auto i = fts.find(inst);
  if(i == fts.end()){
    return nullptr;
  } else {
    return i->second;
  }
}

// result collection
void FuncHandle::addResult(Fetch *f1, Fetch *f2, CheckResult res) {
  auto k = make_pair(f1, f2);
  auto i = result.find(k);

  // add if no result
  if(i == result.end()){
    result.insert(make_pair(k, res));
  }

  // if in higher rank, override
  else if(res < i->second){ 
    result.insert(make_pair(k, res));
  }

  // if error, also record
  if(res == CK_SYMERR){
    failed.insert(k);
  }
}

unsigned FuncHandle::countResult(CheckResult res) {
  unsigned count = 0;

  for(auto &i : result){
    if(i.second == res){
      count++;
    }
  }

  return count;
}

// analysis
void FuncHandle::analyzeFetch(Fetch &fetch) {
#ifdef KSYM_DEBUG
  SLOG.log("inst", Helper::getValueRepr(fetch.inst));
  SLOG.log("host", Helper::getValueName(fetch.inst->getParent()));
#endif

  // collect reachable blocks
  set<BasicBlock *> reach;
  fo.getReachBlocks(fetch.inst->getParent(), reach);

  // create a slice
  Slice slice(reach, fetch.inst);

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_DRAW)
  SLOG.map("slice");
  Record::CFG(slice, SLOG);
  SLOG.pop();
#endif

  // construct analysis
  LLVMSlice wrap(&slice);
  SliceOracle oracle(wrap);

  // unroll
  UnrollPath *unrolled = oracle.getUnrolled(&wrap.getBasisBlock());

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_STAT)
  errs() 
    << func.getName() 
    << "::" << "fetch " << &fetch 
    << "::" << unrolled->size() << " traces" 
    << "\n";
#endif

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_FUNC)
  SLOG.vec("check");
#endif

  // per-trace analysis
  UnrollPath::iterator it = unrolled->begin(), ie = unrolled->end();
  for(; it != ie; ++it){
    analyzeFetchPerTrace(fetch, oracle, *(*it));
  }

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_FUNC)
  SLOG.pop();
#endif
}

static inline void blistToIlist(blist &bl, Instruction *stop, iseq &il) {
  for(LLVMSliceBlock *bb : bl){
    LLVMSliceBlock::inst_iterator i = bb->inst_begin(), ie = bb->inst_end();
    for(; i != ie; ++i){
      il.push_back(*i);
    }
  }

  // trim off from stop inst
  while(il.back() != stop){
    il.pop_back();
  }
}

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_SVAR)
static inline void dumpValueSymbol(Value *val, SEGraph &seg, SymExec &sym) {
  SENode *node = seg.getNodeProbe(val);
  assert(node != nullptr);

  SymVar *svar = sym.getVar(node);
  if(svar == nullptr){
    errs() << "<unknown>\n";
  } else {
    svar->dump(sym.getContext());
  }
}
#endif

void FuncHandle::analyzeFetchPerTrace(Fetch &fetch, SliceOracle &so, 
    blist &blks) {

  // convert to inst list
  iseq trace;
  blistToIlist(blks, fetch.inst, trace);

	// CHENXIONG: start
	/*
	PA *pa = new PA();
	pa->analyzePointTo(trace, so);
	delete pa;
	*/
	// CHENXIONG: end

  try {
    // create SEG
    SEGraph seg(so, trace);

    iseq filt;
    seg.filterTrace(filt);
    //Node::analyzePointTo(trace);

    // create symbol engine
    SymExec sym(mo);

    // symbolize the SEG
    seg.symbolize(sym);

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_SVAR)
    // dump the symbolic results for debugging
    errs() << "---------------- src ----------------\n";
    dumpValueSymbol(fetch.src, seg, sym);

    errs() << "---------------- len ----------------\n";
    dumpValueSymbol(fetch.len, seg, sym);
#endif

    // fetch cross-checking
    int len = trace.size() - 1;

    SENode *fsrc = seg.getNodeOrFail(len, fetch.src);
    SENode *flen = seg.getNodeOrFail(len, fetch.len);

    Fetch *other;
    SENode *osrc, *olen;
    CheckResult res;

    for(int c = 0; c < len; c++){
      Instruction *i = trace.at(c);

      // check if the inst is in SEG
      SENode *node = seg.getNodeOrNull(c, i);
      if(node == nullptr){
        continue;
      }

      // check if the inst is another fetch
      other = getFetchFromInst(i);
      if(other == nullptr){
        continue;
      }

      // check fetch correlation
      osrc = seg.getNodeOrFail(c, other->src);
      olen = seg.getNodeOrFail(c, other->len);

      res = sym.checkOverlap(
          sym.getVar(fsrc), sym.getVar(flen),
          sym.getVar(osrc), sym.getVar(olen));

      addResult(&fetch, other, res);

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_DUMP)
      // log the trace for a potential bug
      if(res == CK_SAT){
        SLOG.map();

        SLOG.log("seq", c);
        SLOG.log("tar", Helper::getValueRepr(other->inst));

        SLOG.map("dfbug");
        SLOG.log("src1", Helper::getValueRepr(fsrc->getVal()));
        SLOG.log("len1", Helper::getValueRepr(flen->getVal()));
        SLOG.log("src2", Helper::getValueRepr(osrc->getVal()));
        SLOG.log("len2", Helper::getValueRepr(olen->getVal()));
        SLOG.pop();

        SLOG.vec("trace");
        seg.replay(sym); 
        SLOG.pop();

        SLOG.pop();
      }
#endif
    }
  } catch(KSymError &e) {
    EXCEPT.insert(fetch.inst->getParent()->getParent());
  }
}

// entry point
void FuncHandle::run() {
#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_DRAW)
  SLOG.map("cfg");
  Record::CFG(func, SLOG);
  SLOG.pop();
#endif

  // collect fetches first 
  collectFetch();

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_STAT)
  if(!fts.empty()){
    errs() 
      << func.getName() 
      << "::" << fts.size() << " fetches" 
      << "\n";
  }
#endif

  // analyze each fetch
#ifdef KSYM_DEBUG
  SLOG.vec("fetch");
#endif

  for(auto &i : fts){
#ifdef KSYM_DEBUG
    SLOG.map();
#endif

    analyzeFetch(*i.second);

#ifdef KSYM_DEBUG
    SLOG.pop();
#endif
  }

#ifdef KSYM_DEBUG
  SLOG.pop();
#endif

#if defined(KSYM_DEBUG) && defined(KSYM_DEBUG_FUNC)
  // log results 
  SLOG.map("result");

  SLOG.log("total", result.size());
  SLOG.log("error", failed.size());
  SLOG.log("sat", countResult(CK_SAT));
  SLOG.log("uns", countResult(CK_UNSAT));
  SLOG.log("udf", countResult(CK_UNDEF));

  SLOG.pop();
#endif
}
