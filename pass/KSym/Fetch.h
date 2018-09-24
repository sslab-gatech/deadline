#ifndef FETCH_H_
#define FETCH_H_

#include "Project.h"

struct FetchDef {
  int src;
  int len;
  int dst;
};

struct Fetch {
  // fields
  Instruction *inst;

  Value *src;
  Value *len;
  Value *dst;

  // methods
  Fetch() {}

  Fetch(Instruction *i, Value *s, Value *l, Value *d)
    : inst(i), src(s), len(l), dst(d) {}

  ~Fetch() {}

  // statics
  static const FetchDef *findDefMatch(const string &name);
};

#endif /* FETCH_H_ */
