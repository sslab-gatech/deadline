#include "Project.h"

// DAG integrity check
void DAItem::verify() {
  for(DAItem *i : preds){
    assert(i->succs.find(this) != i->succs.end());
  }

  for(DAItem *i : succs){
    assert(i->preds.find(this) != i->preds.end());
  }

  check();
}

void DABlock::check() {
  // do nothing
}

void DALoop::check() {
  // ensure single header
  LLVMSliceBlock *hdr = loop->getHeader();

  for(DAItem *p : preds){
    if(DABlock *pb = dyn_cast<DABlock>(p)){
      LLVMSliceBlock::succ_iterator
        si = pb->getBlock()->succ_begin(), se = pb->getBlock()->succ_end();

      for(; si != se; ++si){
        if(loop->contains(*si)){
          assert(*si == hdr);
        }
      }
    }

    else if(DALoop *pl = dyn_cast<DALoop>(p)){
      SmallVector<LLVMSliceBlock *, 32> exits;
      pl->getLoop()->getExitingBlocks(exits);

      for(LLVMSliceBlock *fb : exits){
        LLVMSliceBlock::succ_iterator
          si = fb->succ_begin(), se = fb->succ_end();

        for(; si != se; ++si){
          if(loop->contains(*si)){
            assert(*si == hdr);
          }
        }
      }
    }

    else {
      llvm_unreachable("Unknown DAItem type");
    }
  }
}

void DAGraph::verify() {
  for(DAItem *i : items){
    i->verify();
  }
}

// DAG construction
void DAGraph::build(SliceOracle *so, 
    LLVMSliceLoop *scope, LLVMSliceBlock *header,
    const vector<LLVMSliceBlock *> &blocks) {

  // populate loop info
  LLVMSliceBlock *b;
  LLVMSliceLoop *l;

  for(LLVMSliceBlock *bb : blocks) {
    l = so->getOuterLoopInScope(scope, bb);
    table.insert(make_pair(bb, l));
  }

  // construct the graph
  set<LLVMSliceLoop *> hist;
  for(auto &i : table){
    b = i.first;
    l = i.second;

    if(l == nullptr){
      add(b);
    } else {
      if(hist.find(l) == hist.end()){
        add(l);
        hist.insert(l);
      }
    }
  }

  // populate preds and succs
  DABlock *bnode;
  DALoop *lnode;

  for(auto &i : table){
    b = i.first;
    l = i.second;

    if(l == nullptr){
      bnode = get(b);

      LLVMSliceBlock::succ_iterator si = b->succ_begin(), se = b->succ_end();
      for(; si != se; ++si){
        if(scope != nullptr && !scope->contains(*si)){
          continue;
        }

        auto pos = table.find(*si);
        if(pos->second == nullptr){
          bnode->addSucc(get(pos->first));
        } else {
          bnode->addSucc(get(pos->second));
        }
      }

      LLVMSliceBlock::pred_iterator pi = b->pred_begin(), pe = b->pred_end();
      for(; pi != pe; ++pi){
        if(scope != nullptr && !scope->contains(*pi)){
          continue;
        }

        auto pos = table.find(*pi);
        if(pos->second == nullptr){
          bnode->addPred(get(pos->first));
        } else {
          bnode->addPred(get(pos->second));
        }
      }
    } else {
      lnode = get(l);

      LLVMSliceBlock::succ_iterator si = b->succ_begin(), se = b->succ_end();
      for(; si != se; ++si){
        if(l->contains(*si)){
          continue;
        }

        if(scope != nullptr && !scope->contains(*si)){
          continue;
        }

        auto pos = table.find(*si);
        if(pos->second == nullptr){
          lnode->addSucc(get(pos->first));
        } else {
          lnode->addSucc(get(pos->second));
        }
      }

      LLVMSliceBlock::pred_iterator pi = b->pred_begin(), pe = b->pred_end();
      for(; pi != pe; ++pi){
        if(l->contains(*pi)){
          continue;
        }

        if(scope != nullptr && !scope->contains(*pi)){
          continue;
        }

        auto pos = table.find(*pi);
        if(pos->second == nullptr){
          lnode->addPred(get(pos->first));
        } else {
          lnode->addPred(get(pos->second));
        }
      }
    }
  }

  // finalize
  LLVMSliceBlock *rb = header; 
  LLVMSliceLoop *rl = table[rb];

  if(rl == nullptr){
    root = get(rb);
  } else {
    root = get(rl);
  }

  // verify
  verify();

  // recursively build all sub-graphs in loop nodes
  for(auto const &iter : loops){
    DAGraph *sub = new DAGraph();
    sub->recalculate(iter.first, so);
    subs.insert(make_pair(iter.second, sub));
  }
}

void DAGraph::recalculate(LLVMSlice &s, SliceOracle *so) {
  vector<LLVMSliceBlock *> blocks;
  
  for(LLVMSliceBlock &bb : s){
    blocks.push_back(&bb);
  }

  build(so, nullptr, &s.getEntryBlock(), blocks);
}

void DAGraph::recalculate(LLVMSliceLoop *l, SliceOracle *so) {
  build(so, l, l->getHeader(), l->getBlocks());
}

// Path construction 
static void DFS(DAItem *cur, DAItem *dom, list<DAItem *> &hist, DAPath *path) {
  hist.push_back(cur);

  if(cur == dom){
    path->add(new DATrace(hist));
  } else {
    DAItem::iterator pi = cur->predBegin(), pe = cur->predEnd();
    for(; pi != pe; ++pi){
      DFS(*pi, dom, hist, path);
    }
  }

  hist.pop_back();
}

DAPath *DAGraph::flatten(DAItem *elem) { 
  DAPath *path = new DAPath;

  list<DAItem *> hist;
  DFS(elem, root, hist, path);

  return path;
}
