#ifndef UNROLL_H_
#define UNROLL_H_

#include "Project.h"

typedef list<LLVMSliceBlock *> blist;

class UnrollPath {
  public:
    UnrollPath() {}

    ~UnrollPath() {
      for(blist *i : traces){
        delete i;
      }
    }

    void add(blist *trace) {
      traces.push_back(trace);
    }

    unsigned size() {
      return traces.size();
    }

    typedef typename list<blist *>::iterator iterator;

    iterator begin() {
      return traces.begin();
    }

    iterator end() {
      return traces.end();
    }

  protected:
    list<blist *> traces;
};

class UnrollCache {
  public:
    UnrollCache() {}

    ~UnrollCache() {
      for(auto const &i : cache){
        delete i.second;
      }
    }

    UnrollPath *getUnrolled(DAPath *path, LLVMSliceBlock *term, DAGraph *graph);

  protected:
    void unrollRecursive(DATrace::iterator cur, DATrace::iterator end,
        LLVMSliceBlock *term, DAGraph *graph, blist *blks, UnrollPath *unrolled);

  protected:
    map<pair<DAPath *, LLVMSliceBlock *>, UnrollPath *> cache;
};

#endif /* UNROLL_H_ */
