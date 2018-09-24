#include "Project.h"

void Record::CFG(Function &f, Logger &l) {
  l.log("root", Helper::getValueName(&f.getEntryBlock()));

  l.vec("item");
  for(BasicBlock &bb : f){
    BasicBlock *ptr = &bb;
    l.map();

    l.log("name", Helper::getValueName(ptr));
    l.log("size", ptr->size());

    pred_iterator pi = pred_begin(ptr), pe = pred_end(ptr);
    l.vec("pred");
    for(; pi != pe; ++pi){
      l.log(Helper::getValueName(*pi));
    }
    l.pop();

    succ_iterator si = succ_begin(ptr), se = succ_end(ptr);
    l.vec("succ");
    for(; si != se; ++si){
      l.log(Helper::getValueName(*si));
    }
    l.pop();

    l.pop();
  }
  l.pop();
}

void Record::CFG(Slice &s, Logger &l) {
  l.log("root", Helper::getValueName(s.getEntry()->getBlock()));

  l.vec("item");
  Slice::iterator i = s.begin(), ie = s.end();
  for(; i != ie; ++i){
    SliceBlock *ptr = *i;
    l.map();

    l.log("name", Helper::getValueName(ptr->getBlock()));
    l.log("size", ptr->numInsts());

    SliceBlock::linkIter pi = ptr->predBegin(), pe = ptr->predEnd();
    l.vec("pred");
    for(; pi != pe; ++pi){
      l.log(Helper::getValueName((*pi)->getBlock()));
    }
    l.pop();

    SliceBlock::linkIter si = ptr->succBegin(), se = ptr->succEnd();
    l.vec("succ");
    for(; si != se; ++si){
      l.log(Helper::getValueName((*si)->getBlock()));
    }
    l.pop();

    l.pop();
  }
  l.pop();
}
