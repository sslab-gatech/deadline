#ifndef SEG_H_
#define SEG_H_

// typedefs
typedef vector<Instruction *> iseq;

// type tables
enum SEKind {
  SE_LEAF,
  SE_VAR,
  SE_INST,
  SE_UNKNOWN
};

enum SEType {
  // leaf node
  SE_CInt,
  SE_CNull,

  // var node
  SE_Param,
  SE_Global,
  SE_Local,

  // inst node
  SE_Cast,
  SE_CastTrunc,
  SE_CastZExt,
  SE_CastSExt,
  SE_Cast2Ptr,
  SE_Cast2Int,
  SE_CastType,

  SE_CalcAdd,
  SE_CalcSub,
  SE_CalcMul,
  SE_CalcUDiv,
  SE_CalcSDiv,
  SE_CalcURem,
  SE_CalcSRem,
  SE_CalcShl,
  SE_CalcLShr,
  SE_CalcAShr,
  SE_CalcAnd,
  SE_CalcOr,
  SE_CalcXor,
  SE_Calc,

  SE_CmpEq,
  SE_CmpNe,
  SE_CmpRel,
  SE_Cmp,

  SE_GEPIdx0,
  SE_GEPIdx1,
  SE_GEPIdx2,
  SE_GEP,

  SE_Phi,
  SE_Select,
  SE_Branch,

  SE_Load,
  SE_Store,

#define LIBCALL_ENUM
#include "Libcall.def"
#undef LIBCALL_ENUM
  SE_Call,

#define ASMCALL_ENUM
#include "Asmcall.def"
#undef ASMCALL_ENUM
  SE_Asm,

  SE_ExtVal,

  // others 
  SE_Unhandled
};

// forward declear
class SENode;

// classes
class SEOpv {
  public:
    SEOpv(SEType t, unsigned n, int s, Instruction *i) 
      : type(t), seq(s), inst(i) {

      vals.reserve(n);
    }

    virtual ~SEOpv() {}

    SEType getType() const {
      return type;
    }

    Instruction *getInst() {
      return inst;
    }

    // although addUsr/addDep is defined in SENode, 
    // this is the function to actually implement the linkage
    void setHost(SENode *h); 

    void addVal(SENode *val) {
      vals.push_back(val);
    }

    SENode *at(unsigned i) {
      return vals.at(i);
    }

    // check if this SEOpv is ready to get symbolized
    bool ready(SymExec &sym);

    virtual void symbolize(SymExec &sym, SymVar *var) = 0;

  protected:
    // rtti
    SEType type;

    // basics
    int seq;
    Instruction *inst;

    // links
    SENode *host;
    vector<SENode *> vals;
};

class SEOpv0 : public SEOpv {
  public:
    SEOpv0(SEType t, int s, Instruction *i) 
      : SEOpv(t, 0, s, i) {}

    virtual SymVal *derive(SymExec &sym) = 0;

    void symbolize(SymExec &sym, SymVar *var) override;
};

class SEOpv1 : public SEOpv {
  public:
    SEOpv1(SEType t, int s, Instruction *i,
        SENode *n0) 
      : SEOpv(t, 1, s, i) {

      addVal(n0);
    }
    
    virtual SymVal *derive(SymExec &sym,
        SymVal *i0) = 0;

    void symbolize(SymExec &sym, SymVar *var) override; 
};

class SEOpv2 : public SEOpv {
  public:
    SEOpv2(SEType t, int s, Instruction *i,
        SENode *n0, SENode *n1) 
      : SEOpv(t, 2, s, i) {

      addVal(n0);
      addVal(n1);
    }
    
    virtual SymVal *derive(SymExec &sym, 
        SymVal *i0, SymVal *i1) = 0;

    void symbolize(SymExec &sym, SymVar *var) override; 
};

class SEOpv3 : public SEOpv {
  public:
    SEOpv3(SEType t, int s, Instruction *i,
        SENode *n0, SENode *n1, SENode *n2) 
      : SEOpv(t, 3, s, i) {

      addVal(n0);
      addVal(n1);
      addVal(n2);
    }
    
    virtual SymVal *derive(SymExec &sym, 
        SymVal *i0, SymVal *i1, SymVal *i2) = 0;

    void symbolize(SymExec &sym, SymVar *var) override; 
};

// subclass helpers
#define OPV_0(se_type)                                                \
  class SEOpv##se_type : public SEOpv0 {                              \
    public:                                                           \
      SEOpv##se_type(int s, Instruction *i)                           \
        : SEOpv0(SE_##se_type, s, i) {}                               \
                                                                      \
      ~SEOpv##se_type() {}                                            \
                                                                      \
      static bool classof(const SEOpv *op) {                          \
        return op->getType() == SE_##se_type;                         \
      }                                                               \
                                                                      \
      SymVal *derive(SymExec &sym) override;                          \
  };

#define OPV_1(se_type, op0)                                           \
  class SEOpv##se_type : public SEOpv1 {                              \
    public:                                                           \
      SEOpv##se_type(int s, Instruction *i,                           \
          SENode *op0)                                                \
        : SEOpv1(SE_##se_type, s, i, op0) {}                          \
                                                                      \
      ~SEOpv##se_type() {}                                            \
                                                                      \
      static bool classof(const SEOpv *op) {                          \
        return op->getType() == SE_##se_type;                         \
      }                                                               \
                                                                      \
      SymVal *derive(SymExec &sym,                                    \
          SymVal *i0)                                                 \
          override;                                                   \
  };

#define OPV_2(se_type, op0, op1)                                      \
  class SEOpv##se_type : public SEOpv2 {                              \
    public:                                                           \
      SEOpv##se_type(int s, Instruction *i,                           \
          SENode *op0, SENode *op1)                                   \
        : SEOpv2(SE_##se_type, s, i, op0, op1) {}                     \
                                                                      \
      ~SEOpv##se_type() {}                                            \
                                                                      \
      static bool classof(const SEOpv *op) {                          \
        return op->getType() == SE_##se_type;                         \
      }                                                               \
                                                                      \
      SymVal *derive(SymExec &sym,                                    \
          SymVal *i0, SymVal *i1)                                     \
          override;                                                   \
  };

#define OPV_3(se_type, op0, op1, op2)                                 \
  class SEOpv##se_type : public SEOpv3{                               \
    public:                                                           \
      SEOpv##se_type(int s, Instruction *i,                           \
          SENode *op0, SENode *op1, SENode *op2)                      \
        : SEOpv3(SE_##se_type, s, i, op0, op1, op2) {}                \
                                                                      \
      ~SEOpv##se_type() {}                                            \
                                                                      \
      static bool classof(const SEOpv *op) {                          \
        return op->getType() == SE_##se_type;                         \
      }                                                               \
                                                                      \
      SymVal *derive(SymExec &sym,                                    \
          SymVal *i0, SymVal *i1, SymVal *i2)                         \
          override;                                                   \
  };

class SENode {
  public:
    SENode(SEKind k, SEType t, int s, Value *v, SEGraph *g) 
      : kind(k), type(t), seq(s), val(v), graph(g) {}

    virtual ~SENode() {}

    void setId(unsigned i) {
      id = i;
    }

    unsigned getId() {
      return id;
    }

    int getSeq() {
      return seq;
    }

    Value *getVal() {
      return val;
    }

    SEType getType() const {
      return type;
    }

    SEKind getKind() const {
      return kind;
    }

    SEGraph *getGraph() {
      return graph;
    }

    void addUsr(SENode *n) {
      usrs.insert(n);
    }

    void delUsr(SENode *n) {
      usrs.erase(n);
    }

    void addDep(SENode *n) {
      deps.insert(n);
    }

    void delDep(SENode *n) {
      deps.erase(n);
    }

    unsigned numUsrs() {
      return usrs.size();
    }

    unsigned numDeps() {
      return deps.size();
    }

    typedef typename set<SENode *>::iterator linkIter;

    linkIter usrBegin() {
      return usrs.begin();
    }

    linkIter usrEnd() {
      return usrs.end();
    }

    linkIter depBegin() {
      return deps.begin();
    }

    linkIter depEnd() {
      return deps.end();
    }
    
    virtual SymVar *getSymbol(SymExec &sym) = 0;

  protected:
    // rtti
    SEKind kind;
    SEType type;

    // basics
    unsigned id;
    int seq;
    Value *val;

    // links 
    SEGraph *graph;
    set<SENode *> usrs;
    set<SENode *> deps;
};

class SENodeLeaf : public SENode {
  public:
    SENodeLeaf(SEType t, int s, Value *v, SEGraph *g) 
      : SENode(SE_LEAF, t, s, v, g) {}

    virtual ~SENodeLeaf() {}

    virtual SymVal *derive(SymExec &sym) = 0;

    void symbolize(SymExec &sym, SymVar *var) {
      var->add(derive(sym));
      var->simplify(sym.getContext());
    }

    SymVar *getSymbol(SymExec &sym) override; 

    static bool classof(const SENode *node) {
      return node->getKind() == SE_LEAF;
    }
};

class SENodeVar : public SENode {
  public:
    SENodeVar(SEType t, int s, Value *v, SEGraph *g) 
      : SENode(SE_VAR, t, s, v, g) {}

    virtual ~SENodeVar() {}

    virtual SymVal *derive(SymExec &sym) = 0;

    void symbolize(SymExec &sym, SymVar *var) {
      var->add(derive(sym));
      var->simplify(sym.getContext());
    }

    SymVar *getSymbol(SymExec &sym) override; 

    static bool classof(const SENode *node) {
      return node->getKind() == SE_VAR;
    }
};

class SENodeInst : public SENode {
  public:
    SENodeInst(SEType t, int s, Value *v, SEGraph *g) 
      : SENode(SE_INST, t, s, v, g) {} 

    ~SENodeInst() {
      for(SEOpv *i : opval){
        delete i;
      }
    }

    void addOpv(SEOpv *op) {
      op->setHost(this);
      opval.push_back(op);
    }

    unsigned numOpv() {
      return opval.size();
    }

    SEOpv *getOpv(unsigned i) {
      return opval.at(i);
    }

    SEOpv *getSingleOpv() {
      assert(opval.size() == 1);
      return opval.at(0);
    }

    void symbolize(SymExec &sym, SymVar *var) {
      for(SEOpv *i : opval){
        i->symbolize(sym, var);
      }
      var->simplify(sym.getContext());
    }

    SymVar *getSymbol(SymExec &sym) override; 

    static bool classof(const SENode *node) {
      return node->getKind() == SE_INST;
    }

  protected:
    vector<SEOpv *> opval;
};

class SENodeUnknown : public SENode {
  public:
    SENodeUnknown(int s, Value *v, SEGraph *g) 
      : SENode(SE_UNKNOWN, SE_Unhandled, s, v, g) {}

    ~SENodeUnknown() {}

    SymVar *getSymbol(SymExec &sym) override;

    static bool classof(const SENode *node) {
      return node->getKind() == SE_UNKNOWN;
    }
};

// subclass helpers
#define LEAF_NODE(se_type, val_type)                                  \
  class SENode##se_type : public SENodeLeaf {                         \
    public:                                                           \
      SENode##se_type(int s, val_type *v, SEGraph *g)                 \
        : SENodeLeaf(SE_##se_type, s, v, g) {}                        \
                                                                      \
      ~SENode##se_type() {}                                           \
                                                                      \
      static bool classof(const SENode *node) {                       \
        return node->getType() == SE_##se_type;                       \
      }                                                               \
                                                                      \
      SymVal *derive(SymExec &sym) override;                          \
                                                                      \
    public:                                                           \
      val_type *getCastedVal() {                                      \
        return cast<val_type>(val);                                   \
      }                                                               \
  };

#define VAR_NODE(se_type, val_type)                                   \
  class SENode##se_type : public SENodeVar {                          \
    public:                                                           \
      SENode##se_type(int s, val_type *v, SEGraph *g)                 \
        : SENodeVar(SE_##se_type, s, v, g) {}                         \
                                                                      \
      ~SENode##se_type() {}                                           \
                                                                      \
      static bool classof(const SENode *node) {                       \
        return node->getType() == SE_##se_type;                       \
      }                                                               \
                                                                      \
      SymVal *derive(SymExec &sym) override;                          \
                                                                      \
    public:                                                           \
      val_type *getCastedVal() {                                      \
        return cast<val_type>(val);                                   \
      }                                                               \
  };

#define INST_NODE(se_type, val_type)                                  \
  class SENode##se_type : public SENodeInst {                         \
    public:                                                           \
      SENode##se_type(int s, val_type *v, SEGraph *g)                 \
        : SENodeInst(SE_##se_type, s, v, g) {}                        \
                                                                      \
      ~SENode##se_type() {}                                           \
                                                                      \
      static bool classof(const SENode *node) {                       \
        return node->getType() == SE_##se_type;                       \
      }                                                               \
                                                                      \
    public:                                                           \
      val_type *getCastedVal() {                                      \
        return cast<val_type>(val);                                   \
      }                                                               \
  };

// leaf nodes
LEAF_NODE(CInt, ConstantInt);

LEAF_NODE(CNull, ConstantPointerNull);

// var nodes
VAR_NODE(Param, Argument);

VAR_NODE(Global, GlobalValue);

VAR_NODE(Local, AllocaInst);

// inst nodes and opvals
OPV_1(CastTrunc, Orig);
OPV_1(CastZExt, Orig);
OPV_1(CastSExt, Orig);
OPV_1(Cast2Int, Orig);
OPV_1(Cast2Ptr, Orig);
OPV_1(CastType, Orig);
INST_NODE(Cast, CastInst);

OPV_2(CalcAdd, Lhs, Rhs);
OPV_2(CalcSub, Lhs, Rhs);
OPV_2(CalcMul, Lhs, Rhs);
OPV_2(CalcUDiv, Lhs, Rhs);
OPV_2(CalcSDiv, Lhs, Rhs);
OPV_2(CalcURem, Lhs, Rhs);
OPV_2(CalcSRem, Lhs, Rhs);
OPV_2(CalcShl, Lhs, Rhs);
OPV_2(CalcLShr, Lhs, Rhs);
OPV_2(CalcAShr, Lhs, Rhs);
OPV_2(CalcAnd, Lhs, Rhs);
OPV_2(CalcOr, Lhs, Rhs);
OPV_2(CalcXor, Lhs, Rhs);
INST_NODE(Calc, BinaryOperator)

OPV_2(CmpEq, Lhs, Rhs);
OPV_2(CmpNe, Lhs, Rhs);
OPV_2(CmpRel, Lhs, Rhs);
INST_NODE(Cmp, CmpInst);

OPV_1(GEPIdx0, Ptr);
OPV_2(GEPIdx1, Ptr, Idx0);
OPV_3(GEPIdx2, Ptr, Idx0, Idx1);
INST_NODE(GEP, GetElementPtrInst);

OPV_1(Phi, Tran);
INST_NODE(Phi, PHINode);

OPV_3(Select, CVal, TVal, FVal);
INST_NODE(Select, SelectInst);

OPV_1(Branch, Cval);
INST_NODE(Branch, BranchInst);

OPV_1(Load, Ptr);
INST_NODE(Load, LoadInst);

OPV_2(Store, Ptr, Vop);
INST_NODE(Store, StoreInst);

#define LIBCALL_DOPV
#include "Libcall.def"
#undef LIBCALL_DOPV
INST_NODE(Call, CallInst);

#define ASMCALL_DOPV
#include "Asmcall.def"
#undef ASMCALL_DOPV
INST_NODE(Asm, CallInst);

OPV_1(ExtVal, Ptr);
INST_NODE(ExtVal, ExtractValueInst);

// the SEG graph
class SEGraph {
  public:
    SEGraph(SliceOracle &s, iseq &t) 
      : so(s), trace(t), count(0) {

      followTrace();
      trimGraph();
    } 

    ~SEGraph() {
      for(auto const &i : nodes){
        delete i.second;
      }
    }

    void addNode(SENode *node) {
      auto key = make_pair(node->getSeq(), node->getVal());
      assert(nodes.find(key) == nodes.end());

      node->setId(count++);
      nodes.insert(make_pair(key, node));
    }

    // for get node, seq must match the location of val
    SENode *getNode(int seq, Value *val);

    // for get or build node, seq might not match the location of val,
    // but must be equal of less than the location of val
    SENode *getNodeOrBuild(int cur, Value *val);

    // for get node or null, seq must the location of val
    SENode *getNodeOrNull(int seq, Value *val);

    // for get node or fail, cur might not match the location of val,
    SENode *getNodeOrFail(int cur, Value *val);

    // get the node with the highest in location
    SENode *getNodeProbe(Value *val);

    // node iterator
    typedef typename map<pair<int, Value *>, SENode *>::iterator iterator;

    iterator begin() {
      return nodes.begin();
    }

    iterator end() {
      return nodes.end();
    }

    void filterTrace(iseq &filt);

    // get condition
    int getCond(SENode *node) {
      auto i = conds.find(node);
      if(i == conds.end()){
        return -1;
      } else {
        return i->second;
      }
    }

    // symbols
    void symbolize(SymExec &sym);

    // replay the instruction with the solved model and dump the trace 
    void replay(SymExec &sym);

  protected:
    // locator
    bool locateValue(int cur, Value *val, int &seq);

    // for build node method, cur might match the location of val
    SENode *buildNode(int seq, Value *val);

    // for trace following and triming
    void followTrace();
    void trimGraph();

  protected:
    // info provider
    SliceOracle &so;

    // trace
    iseq &trace;

    // basics
    unsigned count;

    // graph
    map<pair<int, Value *>, SENode *> nodes;

    // conditions
    map<SENode *, int> conds;
};

// helpers
namespace SEGUtil {

static void decompose(GetElementPtrInst *gep, vector<Value *> &vars) {
  Value *ptr = gep->getPointerOperand();

  Value *idx;
  Type *cty, *nty;

  cty = ptr->getType();

  User::op_iterator i = gep->idx_begin(), ie = gep->idx_end();
  for(; i != ie; ++i){
    idx = i->get();

    if(!isa<ConstantInt>(idx)){
      vars.push_back(idx);
    }

    // nothing to GEP on if it is integer type
    assert(!isa<IntegerType>(cty));

    if(isa<PointerType>(cty)){
      nty = cty->getPointerElementType(); 
    }

    else if(isa<StructType>(cty)){
      assert(isa<ConstantInt>(idx));
      unsigned fid = cast<ConstantInt>(idx)->getZExtValue();
      nty = cty->getStructElementType(fid);
    }

    else if(isa<ArrayType>(cty)){
      nty = cty->getArrayElementType();
    }

    else {
      DUMP.typedValue(gep);
      llvm_unreachable("Unhandled GEP type");
    }

    cty = nty;
  }

  // ensure we derefered correctly
  assert(cty == gep->getResultElementType());
}

static Value *backtrace(int seq, PHINode *phi, iseq &trace, SliceOracle &so) {
  SliceBlock *host = so.getSliceHost(phi);
  assert(host != nullptr);

  SliceBlock *prev = nullptr;
  while(--seq >= 0){
    prev = so.getSliceHost(trace.at(seq));
    if(prev != host){
      break;
    }
  }
  assert(prev != nullptr && prev != host);
  assert(host->hasPred(prev) && host->inPTab(prev));

  Value *res = nullptr, *val;
  PHINode::block_iterator bi = phi->block_begin(), be = phi->block_end();
  for(; bi != be; ++bi){
    if(host->inPTab(prev, *bi)){
      val = phi->getIncomingValueForBlock(*bi);

      if(res == nullptr) {
        res = val;
      } else {
        assert(res == val);
      }
    }
  }

  assert(res != nullptr);
  return res;
}

} // end of SEGUtil namespace

#endif /* SEG_H_ */
