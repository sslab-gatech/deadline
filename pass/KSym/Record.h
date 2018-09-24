#ifndef RECORD_H_
#define RECORD_H_

#include "Project.h"

class Record {
  public:
    static void CFG(Function &f, Logger &l);
    static void CFG(Slice &s, Logger &l);
};

#endif /* RECORD_H_ */
