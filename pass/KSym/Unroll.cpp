#include "Project.h"

// parameter to control loop unrolling
// [disabled] #define KSYM_CONFIG_UNROLL_ONCE
#define KSYM_CONFIG_UNROLL_LINK_LIMIT         64
#define KSYM_CONFIG_UNROLL_LATCH_LIMIT        64
#define KSYM_CONFIG_UNROLL_TOTAL_LIMIT        4096

UnrollPath *UnrollCache::getUnrolled(DAPath *path, LLVMSliceBlock *term, 
    DAGraph *graph) {

  auto k = make_pair(path, term);
  
  auto i = cache.find(k);
  if(i != cache.end()){
    return i->second;
  }

  DAItem *mark = graph->query(term);
  assert(mark != nullptr);

  // create new unrolled path
  UnrollPath *unrolled = new UnrollPath;

  DAPath::iterator ti = path->begin(), te = path->end();
  for(; ti != te; ++ti){
    DATrace *trace = *ti;
    assert(mark == *(trace->rbegin()));

    blist *blks = new blist;
    unrollRecursive(trace->begin(), trace->end(), 
        term, graph, blks, unrolled);
  }

  cache.insert(make_pair(k, unrolled));
  return unrolled;
}

static bool isLinked(LLVMSliceBlock *from, LLVMSliceBlock *to) {
  LLVMSliceBlock::succ_iterator si = from->succ_begin(), se = from->succ_end();
  for(; si != se; ++si){
    if(*si == to){
      return true;
    }
  }

  return false;
}

void UnrollCache::unrollRecursive(DATrace::iterator cur, DATrace::iterator end,
    LLVMSliceBlock *term, DAGraph *graph, blist *blks, UnrollPath *unrolled) {

  // test if reached the end of the DATrace
  if(cur == end){
    unrolled->add(blks);
    return;
  }

  // more unrolling needed
  DAItem *item = *cur;
  cur++;

  if(DABlock *dab = dyn_cast<DABlock>(item)){
    blks->push_back(dab->getBlock());
    unrollRecursive(cur, end, term, graph, blks, unrolled);
  }

  else if(DALoop *dal = dyn_cast<DALoop>(item)){
    LLVMSliceLoop *loop = dal->getLoop();
    DAGraph *sub = graph->subgraph(dal);

    // collect and unroll paths to links
    set<LLVMSliceBlock *> links;
    if(cur != end){
      // the loop is not the final step
      SmallVector<LLVMSliceBlock *, 32> exits;
      loop->getExitingBlocks(exits);

      LLVMSliceBlock *next = (*cur)->entrance();
      for(LLVMSliceBlock *e : exits){
        if(isLinked(e, next)){
          links.insert(e);
        }
      }
    } else {
      // the loop is the final step
      links.insert(term);
    }

    map<LLVMSliceBlock *, UnrollPath *> linkPaths;
    for(LLVMSliceBlock *l : links){
      linkPaths.insert(make_pair(l, getUnrolled(sub->getPath(l), l, sub)));
    }

    // pick one link path to unroll the loop
    for(auto const &i : linkPaths){
      UnrollPath *ps = i.second;
      
      unsigned c = 0;
      UnrollPath::iterator pi = ps->begin(), pe = ps->end();
      for(; pi != pe; ++pi){
        if(c++ >= KSYM_CONFIG_UNROLL_TOTAL_LIMIT){
          continue;
        }

        blist *nbl = new blist(*blks);
        nbl->insert(nbl->end(), (*pi)->begin(), (*pi)->end());
        
        unrollRecursive(cur, end, term, graph, nbl, unrolled);
      }
    }

#ifdef KSYM_CONFIG_UNROLL_ONCE
    // unroll paths to latches
    SmallVector<LLVMSliceBlock *, 32> latches;
    loop->getLoopLatches(latches);

    map<LLVMSliceBlock *, UnrollPath *> latchPaths;
    for(LLVMSliceBlock *l : latches){
      latchPaths.insert(make_pair(l, getUnrolled(sub->getPath(l), l, sub)));
    }

    // pick one latch path and one link path to unroll the loop
    for(auto const &i : linkPaths){
      UnrollPath *ips = i.second;
      for(auto const &j : latchPaths){
        UnrollPath *jps = j.second;
      
        unsigned ic = 0;
        UnrollPath::iterator ipi = ips->begin(), ipe = ips->end();
        for(; ipi != ipe; ++ipi){
          if(ic++ >= KSYM_CONFIG_UNROLL_LINK_LIMIT){
            continue;
          }

          unsigned jc = 0;
          UnrollPath::iterator jpi = jps->begin(), jpe = jps->end();
          for(; jpi != jpe; ++jpi){
            if(jc++ >= KSYM_CONFIG_UNROLL_LATCH_LIMIT){
              continue;
            }

            blist *nbl = new blist(*blks);
            nbl->insert(nbl->end(), (*jpi)->begin(), (*jpi)->end());
            nbl->insert(nbl->end(), (*ipi)->begin(), (*ipi)->end());
            
            unrollRecursive(cur, end, term, graph, nbl, unrolled);
          }
        }
      }
    }
#endif

    delete blks;
  }

  else {
    llvm_unreachable("Unknown DAItem type");
  }
}
