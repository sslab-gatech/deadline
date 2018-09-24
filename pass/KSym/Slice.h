#ifndef SLICE_H_
#define SLICE_H_

#include "Project.h"

class SliceBlock {
  public:
    SliceBlock(BasicBlock *bb) : block(bb) {}

    ~SliceBlock() {}

    BasicBlock *getBlock() {
      return block;
    }

    void addInst(Instruction *i) {
      insts.push_back(i);
    }

    typedef typename list<Instruction *>::iterator instIter;

    instIter instBegin() {
      return insts.begin();
    }

    instIter instEnd() {
      return insts.end();
    }

    unsigned numInsts() {
      return insts.size();
    }

    void addPred(SliceBlock *item) {
      preds.push_back(item);
    }

    void addSucc(SliceBlock *item) {
      succs.push_back(item);
    }

    typedef typename vector<SliceBlock *>::iterator linkIter;

    linkIter predBegin() {
      return preds.begin();
    }

    linkIter predEnd() {
      return preds.end();
    }

    linkIter succBegin() {
      return succs.begin();
    }

    linkIter succEnd() {
      return succs.end();
    }

    unsigned numPreds() {
      return preds.size();
    }

    unsigned numSuccs() {
      return succs.size();
    }
    
    bool hasPred(SliceBlock *item) {
      return std::find(preds.begin(), preds.end(), item) != preds.end();
    }

    bool hasSucc(SliceBlock *item) {
      return std::find(succs.begin(), succs.end(), item) != succs.end();
    }

    void addPTabEntry(SliceBlock *item, BasicBlock *bb) {
      ptab[item].insert(bb);
    }

    bool inPTab(SliceBlock *item, BasicBlock *bb) {
      auto i = ptab.find(item);
      assert(i != ptab.end());
      return i->second.find(bb) != i->second.end();
    }

    bool inPTab(SliceBlock *item) {
      return ptab.find(item) != ptab.end();
    }

    bool inPTab(BasicBlock *bb) {
      for(auto const &i : ptab){
        if(i.second.find(bb) != i.second.end()){
          return true;
        }
      }

      return false;
    }

    void inPTab(BasicBlock *bb, set<SliceBlock *> &its) {
      for(auto const &i : ptab){
        if(i.second.find(bb) != i.second.end()){
          its.insert(i.first);
        }
      }
    }

    void addSTabEntry(SliceBlock *item, BasicBlock *bb) {
      stab[item].insert(bb);
    }

    bool inSTab(SliceBlock *item, BasicBlock *bb) {
      auto i = stab.find(item);
      assert(i != stab.end());
      return i->second.find(bb) != i->second.end();
    }

    bool inSTab(SliceBlock *item) {
      return stab.find(item) != stab.end();
    }

    bool inSTab(BasicBlock *bb) {
      for(auto const &i : stab){
        if(i.second.find(bb) != i.second.end()){
          return true;
        }
      }

      return false;
    }

    void inSTab(BasicBlock *bb, set<SliceBlock *> &its) {
      for(auto const &i : stab){
        if(i.second.find(bb) != i.second.end()){
          its.insert(i.first);
        }
      }
    }

    int instPosition(Instruction *inst) {
      int res = 0;
      for(Instruction *i : insts){
        if(i == inst){
          return res;
        }
        i++;
      }

      return -1;
    }

  protected:
    // info
    BasicBlock *block;

    // insts 
    list<Instruction *> insts;
    
    // links
    vector<SliceBlock *> preds;
    vector<SliceBlock *> succs;

    // mapping
    map<SliceBlock *, set<BasicBlock *>> ptab;
    map<SliceBlock *, set<BasicBlock *>> stab;
};

class Slice {
  public:
    Slice(set<BasicBlock *> &pred, Instruction *inst); 

    ~Slice() {
      for(auto const &i : cache){
        delete i.second;
      }
    }

    SliceBlock *getEntry() {
      return entry;
    }

    SliceBlock *getBasis() {
      return basis;
    }

    typedef typename set<SliceBlock *>::iterator iterator;

    iterator begin() {
      return suits.begin();
    }

    iterator end() {
      return suits.end();
    }

    unsigned size() {
      return suits.size();
    }

  protected:
    bool btrace(Value *v);
    void follow(Value *v, set<Value *> &taints);

    SliceBlock *apply(BasicBlock *bb);
    SliceBlock *joint(BasicBlock *bb, list<BasicBlock *> &hists,
        map<BasicBlock *, SliceBlock *> &dedup, set<Value *> &taints);

    void expand(SliceBlock *item);
    void verify();

  protected:
    // scope
    set<BasicBlock *> &reach;
    set<Instruction *> scope;

    // taint 
    set<Value *> backs;
    set<Value *> roots;

    // cache
    SliceBlock *entry;
    SliceBlock *basis;

    map<BasicBlock *, SliceBlock *> cache;
    set<SliceBlock *> suits;
};

// conform to LLVM implementation
namespace llvm {

class LLVMSlice;

class LLVMSliceBlock 
  : public ilist_node_with_parent<LLVMSliceBlock, LLVMSlice> {

  public:
    LLVMSliceBlock(LLVMSlice *slice, SliceBlock *block) 
      : parent(slice), sb(block), bb(block->getBlock()),
        insts(block->instBegin(), block->instEnd()) {} 

    ~LLVMSliceBlock() {}

    // ilist_node requirement
    const LLVMSlice *getParent() const {
      return parent;
    }

    LLVMSlice *getParent() {
      return parent;
    }

    // get backing block
    SliceBlock *getSliceBlock() {
      return sb;
    }

    BasicBlock *getBasicBlock() {
      return bb;
    }

    // inst interation
    typedef list<Instruction *>::iterator inst_iterator;

    inst_iterator inst_begin() {
      return insts.begin();
    }

    inst_iterator inst_end() {
      return insts.end();
    }

    // links
    void addPred(LLVMSliceBlock *block) {
      preds.push_back(block);
    }

    void addSucc(LLVMSliceBlock *block) {
      succs.push_back(block);
    }

    // CFG requirement
    typedef vector<LLVMSliceBlock *>::iterator 
      pred_iterator, succ_iterator;

    typedef vector<LLVMSliceBlock *>::const_iterator 
      const_pred_iterator, const_succ_iterator;

    typedef vector<LLVMSliceBlock *>::reverse_iterator 
      pred_reverse_iterator, succ_reverse_iterator;

    typedef vector<LLVMSliceBlock *>::const_reverse_iterator
      const_pred_reverse_iterator, const_succ_reverse_iterator;

    pred_iterator pred_begin() {
      return preds.begin();
    }

    pred_iterator pred_end() {
      return preds.end();
    }

    const_pred_iterator pred_begin() const {
      return preds.begin();
    }

    const_pred_iterator pred_end() const {
      return preds.end();
    }

    pred_reverse_iterator pred_rbegin() {
      return preds.rbegin();
    }

    pred_reverse_iterator pred_rend() {
      return preds.rend();
    }

    const_pred_reverse_iterator preds_rbegin() const {
      return preds.rbegin();
    }

    const_pred_reverse_iterator preds_rend() const {
      return preds.rend();
    }

    inline iterator_range<pred_iterator> predecessors() {
      return make_range(pred_begin(), pred_end());
    }

    inline iterator_range<const_pred_iterator> predecessors() const {
      return make_range(pred_begin(), pred_end());
    }

    succ_iterator succ_begin() {
      return succs.begin();
    }

    succ_iterator succ_end() {
      return succs.end();
    }

    const_succ_iterator succ_begin() const {
      return succs.begin();
    }

    const_succ_iterator succ_end() const {
      return succs.end();
    }

    succ_reverse_iterator succ_rbegin() {
      return succs.rbegin();
    }

    succ_reverse_iterator succ_rend() {
      return succs.rend();
    }

    const_succ_reverse_iterator succs_rbegin() const {
      return succs.rbegin();
    }

    const_succ_reverse_iterator succs_rend() const {
      return succs.rend();
    }

    inline iterator_range<succ_iterator> successors() {
      return make_range(succ_begin(), succ_end());
    }

    inline iterator_range<const_succ_iterator> successors() const {
      return make_range(succ_begin(), succ_end());
    }

    unsigned pred_size() const {
      return preds.size();
    }

    bool pred_empty() const {
      return preds.empty();
    }

    unsigned succ_size() const {
      return succs.size();
    }

    bool succ_empty() const {
      return succs.empty();
    }

    // print requirement
    void printAsOperand(raw_ostream &OS, bool PrintType = true) const {
      bb->printAsOperand(OS, PrintType);
    }

    void print(raw_ostream &OS, const SlotIndexes * SI = nullptr) const {
      bb->print(OS, SI);
    }

    void print(raw_ostream &OS, ModuleSlotTracker &MST, 
        const SlotIndexes * SI = nullptr) const {

      bb->print(OS, MST, SI);
    }

    void dump() const {
      bb->dump();
    }

  protected:
    // ilist
    LLVMSlice *parent;

    // insts
    SliceBlock *sb;
    BasicBlock *bb;
    list<Instruction *> insts;

    // links
    vector<LLVMSliceBlock *> preds;
    vector<LLVMSliceBlock *> succs;
};

class LLVMSlice {
  public:
    LLVMSlice(Slice *slice); 

    ~LLVMSlice() {}

    // ilist_parent requirement
    static ilist<LLVMSliceBlock> LLVMSlice::*
      getSublistAccess(LLVMSliceBlock *b) {

      return &LLVMSlice::blocks;
    }

    typedef ilist<LLVMSliceBlock>::iterator 
      iterator;

    typedef ilist<LLVMSliceBlock>::const_iterator
      const_iterator;

    typedef ilist<LLVMSliceBlock>::reverse_iterator 
      reverse_iterator;

    typedef ilist<LLVMSliceBlock>::const_reverse_iterator 
      const_reverse_iterator;

    iterator begin() {
      return blocks.begin();
    }

    iterator end() {
      return blocks.end();
    }

    const_iterator begin() const {
      return blocks.begin();
    }

    const_iterator end() const {
      return blocks.end();
    }

    reverse_iterator rbegin() {
      return blocks.rbegin();
    }

    reverse_iterator rend() {
      return blocks.rend();
    }

    const_reverse_iterator rbegin() const {
      return blocks.rbegin();
    }

    const_reverse_iterator rend() const {
      return blocks.rend();
    }

    unsigned size() const {
      return blocks.size();
    }

    bool empty() const {
      return blocks.empty();
    }

    LLVMSliceBlock &front() {
      return blocks.front();
    }

    const LLVMSliceBlock &front() const {
      return blocks.front();
    }

    LLVMSliceBlock &back() {
      return blocks.back();
    }

    const LLVMSliceBlock &back() const {
      return blocks.back();
    }

    LLVMSliceBlock &getEntryBlock() {
      return front();
    }

    const LLVMSliceBlock &getEntryBlock() const {
      return front();
    }

    LLVMSliceBlock &getBasisBlock() {
      return back();
    }

    const LLVMSliceBlock &getBasisBlock() const {
      return back();
    }

  protected:
    ilist<LLVMSliceBlock> blocks;
    map<SliceBlock *, LLVMSliceBlock *> lookup;
};

// specify slice-CFG traits
template<>
struct GraphTraits<LLVMSliceBlock *> {
  typedef LLVMSliceBlock *NodeRef;
  typedef LLVMSliceBlock::succ_iterator ChildIteratorType;

  static NodeRef getEntryNode(LLVMSliceBlock *bb) {
    return bb;
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return N->succ_begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->succ_end();
  }
};

template<>
struct GraphTraits<const LLVMSliceBlock *> {
  typedef const LLVMSliceBlock *NodeRef;
  typedef LLVMSliceBlock::const_succ_iterator ChildIteratorType;

  static NodeRef getEntryNode(LLVMSliceBlock *bb) {
    return bb;
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return N->succ_begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->succ_end();
  }
};

template<>
struct GraphTraits<Inverse<LLVMSliceBlock *>> {
  typedef LLVMSliceBlock *NodeRef;
  typedef LLVMSliceBlock::pred_iterator ChildIteratorType;

  static NodeRef getEntryNode(Inverse<LLVMSliceBlock *> G) {
    return G.Graph;
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return N->pred_begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->pred_end();
  }
};

template<>
struct GraphTraits<Inverse<const LLVMSliceBlock *>> {
  typedef const LLVMSliceBlock *NodeRef;
  typedef LLVMSliceBlock::const_pred_iterator ChildIteratorType;

  static NodeRef getEntryNode(Inverse<const LLVMSliceBlock *> G) {
    return G.Graph;
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return N->pred_begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->pred_end();
  }
};

template<>
struct GraphTraits<LLVMSlice *> 
  : public GraphTraits<LLVMSliceBlock *> {

  static NodeRef getEntryNode(LLVMSlice *S) {
    return &S->front();
  }

  typedef pointer_iterator<LLVMSlice::iterator> nodes_iterator;

  static nodes_iterator nodes_begin(LLVMSlice *S) {
    return nodes_iterator(S->begin());
  }

  static nodes_iterator nodes_end(LLVMSlice *S) {
    return nodes_iterator(S->end());
  }

  static unsigned size (LLVMSlice *S) {
    return S->size();
  }
};

template<>
struct GraphTraits<const LLVMSlice *> 
  : public GraphTraits<const LLVMSliceBlock *> {

  static NodeRef getEntryNode(const LLVMSlice *S) {
    return &S->front();
  }

  typedef pointer_iterator<LLVMSlice::const_iterator> nodes_iterator;

  static nodes_iterator nodes_begin(LLVMSlice *S) {
    return nodes_iterator(S->begin());
  }

  static nodes_iterator nodes_end(LLVMSlice *S) {
    return nodes_iterator(S->end());
  }

  static unsigned size (LLVMSlice *S) {
    return S->size();
  }
};

template<>
struct GraphTraits<Inverse<LLVMSlice *>>
  : public GraphTraits<Inverse<LLVMSliceBlock *>> {

  static NodeRef getEntryNode(Inverse<LLVMSlice *> G) {
    return &G.Graph->front();
  }
};

template<>
struct GraphTraits<Inverse<const LLVMSlice *>>
  : public GraphTraits<Inverse<const LLVMSliceBlock *>> {

  static NodeRef getEntryNode(Inverse<const LLVMSlice *> G) {
    return &G.Graph->front();
  }
};

// template the analysis
template class DomTreeNodeBase<LLVMSliceBlock>;
typedef DomTreeNodeBase<LLVMSliceBlock> SliceDomTreeNode;

template class DominatorTreeBase<LLVMSliceBlock>;
class SliceDomTree : public DominatorTreeBase<LLVMSliceBlock> {
  public:
    SliceDomTree(LLVMSlice &slice) 
      : DominatorTreeBase<LLVMSliceBlock>(false) {

      recalculate(slice);
    }

    ~SliceDomTree() {}
};

template void Calculate<LLVMSlice, LLVMSliceBlock *>(
    DominatorTreeBaseByGraphTraits<GraphTraits<LLVMSliceBlock *>> &DT, 
    LLVMSlice &S);

template void Calculate<LLVMSlice, Inverse<LLVMSliceBlock * >>(
    DominatorTreeBaseByGraphTraits<GraphTraits<Inverse<LLVMSliceBlock *>>> &DT,
    LLVMSlice &S);

class LLVMSliceLoop;
template class LoopBase<LLVMSliceBlock, LLVMSliceLoop>;
class LLVMSliceLoop : public LoopBase<LLVMSliceBlock, LLVMSliceLoop> {
  public:
    LLVMSliceLoop() {}

    LLVMSliceLoop(LLVMSliceBlock *bb) 
      : LoopBase<LLVMSliceBlock, LLVMSliceLoop>(bb) {}
};

template class LoopInfoBase<LLVMSliceBlock, LLVMSliceLoop>;
class SliceLoopInfo : public LoopInfoBase<LLVMSliceBlock, LLVMSliceLoop> {
  public:
    SliceLoopInfo(const SliceDomTree &dt) {
      analyze(dt);
    }

    ~SliceLoopInfo() {}
};

template <>
struct GraphTraits<LLVMSliceLoop *> {
  typedef LLVMSliceLoop *NodeRef;
  typedef SliceLoopInfo::iterator ChildIteratorType;

  static NodeRef getEntryNode(LLVMSliceLoop *L) {
    return L;
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return N->begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->end();
  }
};

template <>
struct GraphTraits<const LLVMSliceLoop *> {
  typedef const LLVMSliceLoop *NodeRef;
  typedef SliceLoopInfo::iterator ChildIteratorType;

  static NodeRef getEntryNode(const LLVMSliceLoop *L) {
    return L;
  }

  static ChildIteratorType child_begin(NodeRef N) {
    return N->begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->end();
  }
};

template <class Node, class ChildIterator>
struct SliceDomTreeGraphTraitsBase {
  typedef Node *NodeRef;
  typedef ChildIterator ChildIteratorType;

  static NodeRef getEntryNode(NodeRef N) {
    return N; 
  }
  
  static ChildIteratorType child_begin(NodeRef N) {
    return N->begin();
  }

  static ChildIteratorType child_end(NodeRef N) {
    return N->end();
  }
};

template <> 
struct GraphTraits<SliceDomTreeNode *> 
  : public SliceDomTreeGraphTraitsBase<SliceDomTreeNode, 
                                       SliceDomTreeNode::iterator> {};

template <>
struct GraphTraits<const SliceDomTreeNode *> 
  : public SliceDomTreeGraphTraitsBase<const SliceDomTreeNode, 
                                       SliceDomTreeNode::const_iterator> {};

template <>
struct GraphTraits<SliceDomTree *>
  : public GraphTraits<SliceDomTreeNode *> {

  static NodeRef getEntryNode(SliceDomTree *DT) {
    return DT->getRootNode();
  }
};

} // end of llvm namespace

#endif /* SLICE_H_ */
