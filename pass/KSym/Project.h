#ifndef PROJECT_H_
#define PROJECT_H_

#include <stdio.h>
#include <stdarg.h>

#include <string>
#include <exception>

#include <set>
#include <map>
#include <queue>
#include <vector>

#include <llvm/Pass.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/Triple.h>
#include <llvm/CodeGen/SlotIndexes.h>

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <llvm/Analysis/CFG.h>

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/Support/GenericDomTree.h>
#include <llvm/Support/GenericDomTreeConstruction.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/LoopInfoImpl.h>

#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/Analysis/TargetLibraryInfo.h>

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/CFLAndersAliasAnalysis.h>
#include <llvm/Analysis/CFLSteensAliasAnalysis.h>
#include <llvm/Analysis/TypeBasedAliasAnalysis.h>
#include <llvm/Analysis/MemoryDependenceAnalysis.h>

#include <z3.h>

#include "json.hpp"

using namespace std;
using namespace llvm;
using json = nlohmann::json;

// existing passes
#include "Lower.h"
#include "Alias.h"

// helper includes
#include "Util.h"
#include "Logger.h"

// project globals
extern Dumper DUMP;

#ifdef KSYM_DEBUG
extern Logger SLOG;
#endif

extern set<Function *> EXCEPT;

// project includes
#include "Slice.h"
#include "DAG.h"
#include "Unroll.h"
#include "Oracle.h"

#include "Symbolic.h"
#include "Trace.h"
#include "SEG.h"

#include "Fetch.h"
#include "Func.h"
#include "Tool.h"

// static recorder that dumps various info for debugging
#include "Record.h"

#endif /* PROJECT_H_ */
