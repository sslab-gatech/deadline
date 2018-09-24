#include "Project.h"

// reachability
void FuncOracle::getReachBlocks(BasicBlock *cur, set<BasicBlock *> &blks) {
  if(blks.find(cur) != blks.end()){
    return;
  }

  blks.insert(cur);

  pred_iterator pi = pred_begin(cur), pe = pred_end(cur);
  for(; pi != pe; ++pi){
    getReachBlocks(*pi, blks);
  }
}

// dominance
LLVMSliceBlock *SliceOracle::getIDom(LLVMSliceBlock *bb) {
  SliceDomTreeNode *node = dt.getNode(bb);
  assert(node != nullptr);

  SliceDomTreeNode *idom = node->getIDom();
  if(idom == nullptr){
    return nullptr;
  }

  return idom->getBlock();
}

// loop info
LLVMSliceLoop *SliceOracle::getOuterLoopInScope(LLVMSliceLoop *scope, 
    LLVMSliceBlock *bb) {

  LLVMSliceLoop *l = li.getLoopFor(bb);
  LLVMSliceLoop *c = nullptr;

  while(l != scope){
    c = l;
    l = l->getParentLoop();
  }

  return c;
}
