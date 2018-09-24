#ifndef TOOL_H_
#define TOOL_H_

#include "Project.h"

class KSym: public ModulePass {
  public:
    static char ID;

    KSym();
    ~KSym(); 

    void getAnalysisUsage(AnalysisUsage &au) const override;
    bool runOnModule(Module &m) override;
};

struct KSymError : public runtime_error {
  Function *func;

  KSymError(Function *f, std::string const& m)
    : std::runtime_error(f->getName().str() + "::" + m), func(f) {}
};

#endif /* TOOL_H_ */
