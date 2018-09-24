#ifndef ALIAS_H_
#define ALIAS_H_

#include "Project.h"

class CombinedAA {
  public:
    CombinedAA(
        const DataLayout &dl, 
        TargetLibraryInfo &tli, 
        AssumptionCache &ac, 
        DominatorTree &dt, 
        LoopInfo &li) : 
      basics(dl, tli, ac, &dt, &li),
      anders(tli),
      steens(tli),
      combo(tli)
    {
      combo.addAAResult(basics);
      combo.addAAResult(anders);
      combo.addAAResult(steens);

      memdep = new MemoryDependenceResults(combo, ac, tli, dt);
    }

    ~CombinedAA() {
      delete memdep;
    }

  protected:
    // available analysis
    BasicAAResult basics;
    CFLAndersAAResult anders;
    CFLSteensAAResult steens;

    // worker
    AAResults combo;
    MemoryDependenceResults *memdep;
};

#endif /* ALIAS_H_ */
