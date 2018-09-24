#ifndef ORACLE_H_
#define ORACLE_H_

#include "Project.h"

class SliceOracle {
  public:
    SliceOracle(LLVMSlice &s) : dt(s), li(dt), dag() {
      dag.recalculate(s, this);

      SliceBlock *sb;
      for(LLVMSliceBlock &lb : s){
        sb = lb.getSliceBlock(); 

        LLVMSliceBlock::inst_iterator 
          ii = lb.inst_begin(), ie = lb.inst_end();

        for(; ii != ie; ++ii){
          insts.insert(make_pair(*ii, sb));
        }
      }
    }

    ~SliceOracle() {}

    // mapping
    SliceBlock *getSliceHost(Instruction *inst) {
      auto i = insts.find(inst);
      if(i == insts.end()){
        return nullptr;
      } else {
        return i->second;
      }
    }

    // dominance
    bool dominates(LLVMSliceBlock *dom, LLVMSliceBlock *bb) {
      return dt.dominates(dom, bb);
    }

    LLVMSliceBlock *getIDom(LLVMSliceBlock *bb); 

    // loops
    LLVMSliceLoop *getOuterLoopInScope(LLVMSliceLoop *scope, 
        LLVMSliceBlock *bb);

    LLVMSliceLoop *getInnerLoop(LLVMSliceBlock *bb) {
      return li.getLoopFor(bb);
    }

    LLVMSliceLoop *getOuterLoop(LLVMSliceBlock *bb) {
      return getOuterLoopInScope(nullptr, bb);
    }

    // DAG 
    DAPath *getPath(LLVMSliceBlock *bb) {
      return dag.getPath(bb);
    }

    UnrollPath *getUnrolled(LLVMSliceBlock *bb) {
      DAPath *path = getPath(bb);
      assert(path->size() != 0);
      return uc.getUnrolled(path, bb, &dag);
    }

  protected:
    // mapping
    map<Instruction *, SliceBlock *> insts;

    // basics
    SliceDomTree dt;
    SliceLoopInfo li;

    // DAG
    DAGraph dag;
    UnrollCache uc;
};

class FuncOracle {
  public:
    FuncOracle(Function &f, 
        const DataLayout &dl, TargetLibraryInfo &tli) :
      ac(f),
      dt(f),
      li(dt),
      caa(dl, tli, ac, dt, li)
    {}

    ~FuncOracle() {}

    // reachability
    void getReachBlocks(BasicBlock *cur, set<BasicBlock *> &blks);

  protected:
    AssumptionCache ac;
    DominatorTree dt;
    LoopInfo li;
    CombinedAA caa;
};

class ModuleOracle {
  public:
    ModuleOracle(Module &m) : 
      dl(m.getDataLayout()),
      tli(TargetLibraryInfoImpl(Triple(Twine(m.getTargetTriple()))))
    {}

    ~ModuleOracle() {}

    // getter
    const DataLayout &getDataLayout() {
      return dl;
    }

    TargetLibraryInfo &getTargetLibraryInfo() {
      return tli;
    }

    // data layout
    uint64_t getBits() {
      return BITS;
    }

    uint64_t getPointerWidth() {
      return dl.getPointerSizeInBits();
    }

    uint64_t getPointerSize() {
      return dl.getPointerSize();
    }

    uint64_t getTypeSize(Type *ty) {
      return dl.getTypeAllocSize(ty);
    }

    uint64_t getTypeWidth(Type *ty) {
      return dl.getTypeSizeInBits(ty);
    }

    uint64_t getTypeOffset(Type *type, unsigned idx) {
      assert(isa<StructType>(type));
      return dl.getStructLayout(cast<StructType>(type))->getElementOffset(idx);
    }

    bool isReintPointerType(Type *ty) {
      return ty->isPointerTy() || 
        (ty->isIntegerTy() && ty->getIntegerBitWidth() == getPointerWidth());
    }

  protected:
    // info provider
    const DataLayout &dl;
    TargetLibraryInfo tli;

    // consts
    const uint64_t BITS = 8;
};

#endif /* ORACLE_H_ */
