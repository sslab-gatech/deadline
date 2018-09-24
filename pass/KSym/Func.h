#ifndef FUNC_H_
#define FUNC_H_

#include "Project.h"

class FuncHandle {
  public:
    FuncHandle(Function &f, ModuleOracle &m) 
      : func(f), mo(m), fo(f, m.getDataLayout(), m.getTargetLibraryInfo()) 
    {}

    ~FuncHandle() {
      for(auto const &i : fts){
        delete i.second;
      }
    } 

    void run();

  protected:
    // fetch collection
    void collectFetch();
    Fetch *getFetchFromInst(Instruction *inst);

    // fetch analysis
    void analyzeFetch(Fetch &fetch);
    void analyzeFetchPerTrace(Fetch &fetch, SliceOracle &so, blist &blks);

    // fetch results
    void addResult(Fetch *f1, Fetch *f2, CheckResult res); 
    unsigned countResult(CheckResult res);

  protected:
    // context
    Function &func;

    // oracle
    ModuleOracle &mo;
    FuncOracle fo;

    // fetches
    map<Instruction *, Fetch *> fts;
    
    // results
    map<pair<Fetch *, Fetch *>, CheckResult> result;
    set<pair<Fetch *, Fetch *>> failed;
};

#endif /* FUNC_H_ */
