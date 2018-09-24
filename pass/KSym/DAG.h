#ifndef DAG_H_
#define DAG_H_

#include "Project.h"

// forward decleration
class SliceOracle; 

// classes
class DAItem {
  public:
    enum DAItemType {
      DA_BLOCK,
      DA_LOOP
    };

  public:
    DAItem(DAItemType t) : type(t) {} 

    ~DAItem() {}

    DAItemType getType() const {
      return type;
    }

    void addPred(DAItem *item) {
      preds.insert(item);
    }

    void addSucc(DAItem *item) {
      succs.insert(item);
    }

    typedef typename set<DAItem *>::iterator iterator;

    iterator predBegin() {
      return preds.begin();
    }

    iterator predEnd() {
      return preds.end();
    }

    iterator succBegin() {
      return succs.begin();
    }

    iterator succEnd() {
      return succs.end();
    }

    unsigned numPreds() {
      return preds.size();
    }

    unsigned numSuccs() {
      return succs.size();
    }

    virtual void check() = 0;

    void verify(); 

    virtual LLVMSliceBlock *entrance() = 0;

  protected:
    // rtti
    DAItemType type;

    // links
    set<DAItem *> preds;
    set<DAItem *> succs;
};

class DABlock : public DAItem {
  public:
    DABlock(LLVMSliceBlock *b) : DAItem(DA_BLOCK), bb(b) {}

    ~DABlock() {}

    LLVMSliceBlock *getBlock() {
      return bb;
    }

    static bool classof(const DAItem *item) {
      return item->getType() == DA_BLOCK;
    }

    void check() override; 

    LLVMSliceBlock *entrance() override {
      return bb;
    }

  protected:
    LLVMSliceBlock *bb;
};

class DALoop : public DAItem {
  public:
    DALoop(LLVMSliceLoop *l) : DAItem(DA_LOOP), loop(l) {}

    ~DALoop() {}

    LLVMSliceLoop *getLoop() {
      return loop;
    }

    static bool classof(const DAItem *item) {
      return item->getType() == DA_LOOP;
    }

    void check () override; 

    LLVMSliceBlock *entrance() override {
      return loop->getHeader();
    }

  protected:
    LLVMSliceLoop *loop;
};

class DATrace {
  public:
    DATrace(list<DAItem *> &items) : steps(items) {}

    ~DATrace() {}

     // NOTE: internally the steps from A to B are stored in a reverse 
     // order (i.e., B->...->A) so we flip the iterator to compendate this

    typedef typename list<DAItem *>::reverse_iterator iterator;

    iterator begin() {
      return steps.rbegin();
    }

    iterator end() {
      return steps.rend();
    }

    typedef typename list<DAItem *>::iterator reverse_iterator;

    reverse_iterator rbegin() {
      return steps.begin();
    }

    reverse_iterator rend() {
      return steps.end();
    }

    unsigned size() {
      return steps.size();
    }

  protected:
    list<DAItem *> steps;
};

class DAPath {
  public:
    DAPath() {}

    ~DAPath() {
      for(DATrace *trace : traces){
        delete trace;
      }
    }

    void add(DATrace *trace) {
      traces.push_back(trace);
    }

    unsigned size() {
      return traces.size();
    }

    typedef typename list<DATrace *>::iterator iterator;

    iterator begin() {
      return traces.begin();
    }

    iterator end() {
      return traces.end();
    }

  protected:
    list<DATrace *> traces;
};

class DAGraph {
  public:
    DAGraph() : root(nullptr) {}

    ~DAGraph() {
      for(auto const &i : subs){
        delete i.second;
      }

      for(auto const &i : blocks){
        delete i.second;
      }

      for(auto const &i : loops){
        delete i.second;
      }

      for(auto const &i : paths){
        delete i.second;
      }
    }

    void recalculate(LLVMSlice &s, SliceOracle *so);

    void recalculate(LLVMSliceLoop *l, SliceOracle *so);

    void verify(); 

    DAItem *query(LLVMSliceBlock *b) {
      auto pos = table.find(b);
      if(pos == table.end()){
        return nullptr;
      }

      if(pos->second == nullptr){
        return get(b);
      } else {
        return get(pos->second);
      }
    }

    DAGraph *subgraph(DALoop *l) {
      auto pos = subs.find(l);
      if(pos == subs.end()){
        return nullptr;
      }

      return pos->second;
    }

    unsigned size() {
      return items.size();
    }

    typedef typename list<DAItem *>::iterator iterator;

    iterator begin() {
      return items.begin();
    }

    iterator end() {
      return items.end();
    }

    DAPath *getPath(DAItem *elem) {
      auto i = paths.find(elem);
      if(i != paths.end()){
        return i->second;
      }

      DAPath *res = flatten(elem);
      paths.insert(make_pair(elem, res));
      return res;
    }

    DAPath *getPath(LLVMSliceBlock *bb) {
      DAItem *elem = query(bb);
      assert(elem != nullptr);
      return getPath(elem);
    }

  protected:
    void add(LLVMSliceBlock *b) {
      DABlock *block = new DABlock(b);
      blocks.insert(make_pair(b, block));
      items.push_back(block);
    }

    void add(LLVMSliceLoop *l) {
      DALoop *loop = new DALoop(l);
      loops.insert(make_pair(l, loop));
      items.push_back(loop);
    }

    void build(SliceOracle *so, LLVMSliceLoop *scope, LLVMSliceBlock *header, 
        const vector<LLVMSliceBlock *> &blocks);

    DABlock *get(LLVMSliceBlock *b) {
      auto pos = blocks.find(b);
      if(pos == blocks.end()){
        return nullptr;
      }

      return pos->second;
    }

    DALoop *get(LLVMSliceLoop *l) {
      auto pos = loops.find(l);
      if(pos == loops.end()){
        return nullptr;
      }

      return pos->second;
    }

    DAPath *flatten(DAItem *elem);

  protected:
    // basics
    DAItem *root;
    list<DAItem *> items;

    // mappings 
    map<LLVMSliceBlock *, LLVMSliceLoop *> table;

    map<LLVMSliceBlock *, DABlock *> blocks;
    map<LLVMSliceLoop *, DALoop *> loops;

    // enclused graphs
    map<DALoop *, DAGraph *> subs;

    // path repository
    map<DAItem *, DAPath *> paths;
};

#endif /* DAG_H_ */
