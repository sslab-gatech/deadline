// adapted from LowerSwitch.cpp

#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include <algorithm>
using namespace llvm;

#define DEBUG_TYPE "lower-pass"

// the lower-switch pass
namespace {
  struct IntRange {
    int64_t Low, High;
  };
  // Return true iff R is covered by Ranges.
  static bool IsInRanges(const IntRange &R,
                         const std::vector<IntRange> &Ranges) {
    // Note: Ranges must be sorted, non-overlapping and non-adjacent.

    // Find the first range whose High field is >= R.High,
    // then check if the Low field is <= R.Low. If so, we
    // have a Range that covers R.
    auto I = std::lower_bound(
        Ranges.begin(), Ranges.end(), R,
        [](const IntRange &A, const IntRange &B) { return A.High < B.High; });
    return I != Ranges.end() && I->Low <= R.Low;
  }

  /// Replace all SwitchInst instructions with chained branch instructions.
  class LowerSwitch {
  public:
    LowerSwitch() {} 

    bool runOnFunction(Function &F);

    struct CaseRange {
      ConstantInt* Low;
      ConstantInt* High;
      BasicBlock* BB;

      CaseRange(ConstantInt *low, ConstantInt *high, BasicBlock *bb)
          : Low(low), High(high), BB(bb) {}
    };

    typedef std::vector<CaseRange> CaseVector;
    typedef std::vector<CaseRange>::iterator CaseItr;

  private:
    void processSwitchInst(SwitchInst *SI, 
        SmallPtrSetImpl<BasicBlock*> &DeleteList);

    BasicBlock *switchConvert(CaseItr Begin, CaseItr End,
                              ConstantInt *LowerBound, ConstantInt *UpperBound,
                              Value *Val, BasicBlock *Predecessor,
                              BasicBlock *OrigBlock, BasicBlock *Default,
                              const std::vector<IntRange> &UnreachableRanges);

    BasicBlock *newLeafBlock(CaseRange &Leaf, Value *Val, 
                             BasicBlock *OrigBlock, BasicBlock *Default);

    unsigned Clusterify(CaseVector &Cases, SwitchInst *SI);
  };

  /// The comparison function for sorting the switch case values in the vector.
  /// WARNING: Case ranges should be disjoint!
  struct CaseCmp {
    bool operator () (const LowerSwitch::CaseRange& C1,
                      const LowerSwitch::CaseRange& C2) {

      const ConstantInt* CI1 = cast<const ConstantInt>(C1.Low);
      const ConstantInt* CI2 = cast<const ConstantInt>(C2.High);
      return CI1->getValue().slt(CI2->getValue());
    }
  };
}

bool LowerSwitch::runOnFunction(Function &F) {
  bool Changed = false;
  SmallPtrSet<BasicBlock*, 8> DeleteList;

  for (Function::iterator I = F.begin(), E = F.end(); I != E; ) {
    BasicBlock *Cur = &*I++; // Advance over block so we don't traverse new blocks

    // If the block is a dead Default block that will be deleted later, don't
    // waste time processing it.
    if (DeleteList.count(Cur))
      continue;

    if (SwitchInst *SI = dyn_cast<SwitchInst>(Cur->getTerminator())) {
      Changed = true;
      processSwitchInst(SI, DeleteList);
    }
  }

  for (BasicBlock* BB: DeleteList) {
    DeleteDeadBlock(BB);
  }

  return Changed;
}

/// Used for debugging purposes.
static raw_ostream& operator<<(raw_ostream &O,
                               const LowerSwitch::CaseVector &C)
    LLVM_ATTRIBUTE_USED;
static raw_ostream& operator<<(raw_ostream &O,
                               const LowerSwitch::CaseVector &C) {
  O << "[";

  for (LowerSwitch::CaseVector::const_iterator B = C.begin(),
         E = C.end(); B != E; ) {
    O << *B->Low << " -" << *B->High;
    if (++B != E) O << ", ";
  }

  return O << "]";
}

/// \brief Update the first occurrence of the "switch statement" BB in the PHI
/// node with the "new" BB. The other occurrences will:
///
/// 1) Be updated by subsequent calls to this function.  Switch statements may
/// have more than one outcoming edge into the same BB if they all have the same
/// value. When the switch statement is converted these incoming edges are now
/// coming from multiple BBs.
/// 2) Removed if subsequent incoming values now share the same case, i.e.,
/// multiple outcome edges are condensed into one. This is necessary to keep the
/// number of phi values equal to the number of branches to SuccBB.
static void fixPhis(BasicBlock *SuccBB, BasicBlock *OrigBB, BasicBlock *NewBB,
                    unsigned NumMergedCases) {
  for (BasicBlock::iterator I = SuccBB->begin(),
                            IE = SuccBB->getFirstNonPHI()->getIterator();
       I != IE; ++I) {
    PHINode *PN = cast<PHINode>(I);

    // Only update the first occurrence.
    unsigned Idx = 0, E = PN->getNumIncomingValues();
    unsigned LocalNumMergedCases = NumMergedCases;
    for (; Idx != E; ++Idx) {
      if (PN->getIncomingBlock(Idx) == OrigBB) {
        PN->setIncomingBlock(Idx, NewBB);
        break;
      }
    }

    // Remove additional occurrences coming from condensed cases and keep the
    // number of incoming values equal to the number of branches to SuccBB.
    SmallVector<unsigned, 8> Indices;
    for (++Idx; LocalNumMergedCases > 0 && Idx < E; ++Idx)
      if (PN->getIncomingBlock(Idx) == OrigBB) {
        Indices.push_back(Idx);
        LocalNumMergedCases--;
      }
    // Remove incoming values in the reverse order to prevent invalidating
    // *successive* index.
    for (unsigned III : reverse(Indices))
      PN->removeIncomingValue(III);
  }
}

/// Convert the switch statement into a binary lookup of the case values.
/// The function recursively builds this tree. LowerBound and UpperBound are
/// used to keep track of the bounds for Val that have already been checked by
/// a block emitted by one of the previous calls to switchConvert in the call
/// stack.
BasicBlock *
LowerSwitch::switchConvert(CaseItr Begin, CaseItr End, ConstantInt *LowerBound,
                           ConstantInt *UpperBound, Value *Val,
                           BasicBlock *Predecessor, BasicBlock *OrigBlock,
                           BasicBlock *Default,
                           const std::vector<IntRange> &UnreachableRanges) {
  unsigned Size = End - Begin;

  if (Size == 1) {
    // Check if the Case Range is perfectly squeezed in between
    // already checked Upper and Lower bounds. If it is then we can avoid
    // emitting the code that checks if the value actually falls in the range
    // because the bounds already tell us so.
    if (Begin->Low == LowerBound && Begin->High == UpperBound) {
      unsigned NumMergedCases = 0;
      if (LowerBound && UpperBound)
        NumMergedCases =
            UpperBound->getSExtValue() - LowerBound->getSExtValue();
      fixPhis(Begin->BB, OrigBlock, Predecessor, NumMergedCases);
      return Begin->BB;
    }
    return newLeafBlock(*Begin, Val, OrigBlock, Default);
  }

  unsigned Mid = Size / 2;
  std::vector<CaseRange> LHS(Begin, Begin + Mid);
  DEBUG(dbgs() << "LHS: " << LHS << "\n");
  std::vector<CaseRange> RHS(Begin + Mid, End);
  DEBUG(dbgs() << "RHS: " << RHS << "\n");

  CaseRange &Pivot = *(Begin + Mid);
  DEBUG(dbgs() << "Pivot ==> "
               << Pivot.Low->getValue()
               << " -" << Pivot.High->getValue() << "\n");

  // NewLowerBound here should never be the integer minimal value.
  // This is because it is computed from a case range that is never
  // the smallest, so there is always a case range that has at least
  // a smaller value.
  ConstantInt *NewLowerBound = Pivot.Low;

  // Because NewLowerBound is never the smallest representable integer
  // it is safe here to subtract one.
  ConstantInt *NewUpperBound = ConstantInt::get(NewLowerBound->getContext(),
                                                NewLowerBound->getValue() - 1);

  if (!UnreachableRanges.empty()) {
    // Check if the gap between LHS's highest and NewLowerBound is unreachable.
    int64_t GapLow = LHS.back().High->getSExtValue() + 1;
    int64_t GapHigh = NewLowerBound->getSExtValue() - 1;
    IntRange Gap = { GapLow, GapHigh };
    if (GapHigh >= GapLow && IsInRanges(Gap, UnreachableRanges))
      NewUpperBound = LHS.back().High;
  }

  DEBUG(dbgs() << "LHS Bounds ==> ";
        if (LowerBound) {
          dbgs() << LowerBound->getSExtValue();
        } else {
          dbgs() << "NONE";
        }
        dbgs() << " - " << NewUpperBound->getSExtValue() << "\n";
        dbgs() << "RHS Bounds ==> ";
        dbgs() << NewLowerBound->getSExtValue() << " - ";
        if (UpperBound) {
          dbgs() << UpperBound->getSExtValue() << "\n";
        } else {
          dbgs() << "NONE\n";
        });

  // Create a new node that checks if the value is < pivot. Go to the
  // left branch if it is and right branch if not.
  Function* F = OrigBlock->getParent();
  BasicBlock* NewNode = BasicBlock::Create(Val->getContext(), "NodeBlock");

  ICmpInst* Comp = new ICmpInst(ICmpInst::ICMP_SLT,
                                Val, Pivot.Low, "Pivot");

  BasicBlock *LBranch = switchConvert(LHS.begin(), LHS.end(), LowerBound,
                                      NewUpperBound, Val, NewNode, OrigBlock,
                                      Default, UnreachableRanges);
  BasicBlock *RBranch = switchConvert(RHS.begin(), RHS.end(), NewLowerBound,
                                      UpperBound, Val, NewNode, OrigBlock,
                                      Default, UnreachableRanges);

  F->getBasicBlockList().insert(++OrigBlock->getIterator(), NewNode);
  NewNode->getInstList().push_back(Comp);

  BranchInst::Create(LBranch, RBranch, Comp, NewNode);
  return NewNode;
}

/// Create a new leaf block for the binary lookup tree. It checks if the
/// switch's value == the case's value. If not, then it jumps to the default
/// branch. At this point in the tree, the value can't be another valid case
/// value, so the jump to the "default" branch is warranted.
BasicBlock* LowerSwitch::newLeafBlock(CaseRange& Leaf, Value* Val,
                                      BasicBlock* OrigBlock,
                                      BasicBlock* Default)
{
  Function* F = OrigBlock->getParent();
  BasicBlock* NewLeaf = BasicBlock::Create(Val->getContext(), "LeafBlock");
  F->getBasicBlockList().insert(++OrigBlock->getIterator(), NewLeaf);

  // Emit comparison
  ICmpInst* Comp = nullptr;
  if (Leaf.Low == Leaf.High) {
    // Make the seteq instruction...
    Comp = new ICmpInst(*NewLeaf, ICmpInst::ICMP_EQ, Val,
                        Leaf.Low, "SwitchLeaf");
  } else {
    // Make range comparison
    if (Leaf.Low->isMinValue(true /*isSigned*/)) {
      // Val >= Min && Val <= Hi --> Val <= Hi
      Comp = new ICmpInst(*NewLeaf, ICmpInst::ICMP_SLE, Val, Leaf.High,
                          "SwitchLeaf");
    } else if (Leaf.Low->isZero()) {
      // Val >= 0 && Val <= Hi --> Val <=u Hi
      Comp = new ICmpInst(*NewLeaf, ICmpInst::ICMP_ULE, Val, Leaf.High,
                          "SwitchLeaf");      
    } else {
      // Emit V-Lo <=u Hi-Lo
      Constant* NegLo = ConstantExpr::getNeg(Leaf.Low);
      Instruction* Add = BinaryOperator::CreateAdd(Val, NegLo,
                                                   Val->getName()+".off",
                                                   NewLeaf);
      Constant *UpperBound = ConstantExpr::getAdd(NegLo, Leaf.High);
      Comp = new ICmpInst(*NewLeaf, ICmpInst::ICMP_ULE, Add, UpperBound,
                          "SwitchLeaf");
    }
  }

  // Make the conditional branch...
  BasicBlock* Succ = Leaf.BB;
  BranchInst::Create(Succ, Default, Comp, NewLeaf);

  // If there were any PHI nodes in this successor, rewrite one entry
  // from OrigBlock to come from NewLeaf.
  for (BasicBlock::iterator I = Succ->begin(); isa<PHINode>(I); ++I) {
    PHINode* PN = cast<PHINode>(I);
    // Remove all but one incoming entries from the cluster
    uint64_t Range = Leaf.High->getSExtValue() -
                     Leaf.Low->getSExtValue();
    for (uint64_t j = 0; j < Range; ++j) {
      PN->removeIncomingValue(OrigBlock);
    }
    
    int BlockIdx = PN->getBasicBlockIndex(OrigBlock);
    assert(BlockIdx != -1 && "Switch didn't go to this successor??");
    PN->setIncomingBlock((unsigned)BlockIdx, NewLeaf);
  }

  return NewLeaf;
}

/// Transform simple list of Cases into list of CaseRange's.
unsigned LowerSwitch::Clusterify(CaseVector& Cases, SwitchInst *SI) {
  unsigned numCmps = 0;

  // Start with "simple" cases
  for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i)
    Cases.push_back(CaseRange(i.getCaseValue(), i.getCaseValue(),
                              i.getCaseSuccessor()));
  
  std::sort(Cases.begin(), Cases.end(), CaseCmp());

  // Merge case into clusters
  if (Cases.size() >= 2) {
    CaseItr I = Cases.begin();
    for (CaseItr J = std::next(I), E = Cases.end(); J != E; ++J) {
      int64_t nextValue = J->Low->getSExtValue();
      int64_t currentValue = I->High->getSExtValue();
      BasicBlock* nextBB = J->BB;
      BasicBlock* currentBB = I->BB;

      // If the two neighboring cases go to the same destination, merge them
      // into a single case.
      assert(nextValue > currentValue && "Cases should be strictly ascending");
      if ((nextValue == currentValue + 1) && (currentBB == nextBB)) {
        I->High = J->High;
        // FIXME: Combine branch weights.
      } else if (++I != J) {
        *I = *J;
      }
    }
    Cases.erase(std::next(I), Cases.end());
  }

  for (CaseItr I=Cases.begin(), E=Cases.end(); I!=E; ++I, ++numCmps) {
    if (I->Low != I->High)
      // A range counts double, since it requires two compares.
      ++numCmps;
  }

  return numCmps;
}

/// Replace the specified switch instruction with a sequence of chained if-then
/// insts in a balanced binary search.
void LowerSwitch::processSwitchInst(SwitchInst *SI,
                                    SmallPtrSetImpl<BasicBlock*> &DeleteList) {
  BasicBlock *CurBlock = SI->getParent();
  BasicBlock *OrigBlock = CurBlock;
  Function *F = CurBlock->getParent();
  Value *Val = SI->getCondition();  // The value we are switching on...
  BasicBlock* Default = SI->getDefaultDest();

  // Don't handle unreachable blocks. If there are successors with phis, this
  // would leave them behind with missing predecessors.
  if ((CurBlock != &F->getEntryBlock() && pred_empty(CurBlock)) ||
      CurBlock->getSinglePredecessor() == CurBlock) {
    DeleteList.insert(CurBlock);
    return;
  }

  // If there is only the default destination, just branch.
  if (!SI->getNumCases()) {
    BranchInst::Create(Default, CurBlock);
    SI->eraseFromParent();
    return;
  }

  // Prepare cases vector.
  CaseVector Cases;
  unsigned numCmps = Clusterify(Cases, SI);
  DEBUG(dbgs() << "Clusterify finished. Total clusters: " << Cases.size()
               << ". Total compares: " << numCmps << "\n");
  DEBUG(dbgs() << "Cases: " << Cases << "\n");
  (void)numCmps;

  ConstantInt *LowerBound = nullptr;
  ConstantInt *UpperBound = nullptr;
  std::vector<IntRange> UnreachableRanges;

  if (isa<UnreachableInst>(Default->getFirstNonPHIOrDbg())) {
    // Make the bounds tightly fitted around the case value range, because we
    // know that the value passed to the switch must be exactly one of the case
    // values.
    assert(!Cases.empty());
    LowerBound = Cases.front().Low;
    UpperBound = Cases.back().High;

    DenseMap<BasicBlock *, unsigned> Popularity;
    unsigned MaxPop = 0;
    BasicBlock *PopSucc = nullptr;

    IntRange R = { INT64_MIN, INT64_MAX };
    UnreachableRanges.push_back(R);
    for (const auto &I : Cases) {
      int64_t Low = I.Low->getSExtValue();
      int64_t High = I.High->getSExtValue();

      IntRange &LastRange = UnreachableRanges.back();
      if (LastRange.Low == Low) {
        // There is nothing left of the previous range.
        UnreachableRanges.pop_back();
      } else {
        // Terminate the previous range.
        assert(Low > LastRange.Low);
        LastRange.High = Low - 1;
      }
      if (High != INT64_MAX) {
        IntRange R = { High + 1, INT64_MAX };
        UnreachableRanges.push_back(R);
      }

      // Count popularity.
      int64_t N = High - Low + 1;
      unsigned &Pop = Popularity[I.BB];
      if ((Pop += N) > MaxPop) {
        MaxPop = Pop;
        PopSucc = I.BB;
      }
    }
#ifndef NDEBUG
    /* UnreachableRanges should be sorted and the ranges non-adjacent. */
    for (auto I = UnreachableRanges.begin(), E = UnreachableRanges.end();
         I != E; ++I) {
      assert(I->Low <= I->High);
      auto Next = I + 1;
      if (Next != E) {
        assert(Next->Low > I->High);
      }
    }
#endif

    // Use the most popular block as the new default, reducing the number of
    // cases.
    assert(MaxPop > 0 && PopSucc);
    Default = PopSucc;
    Cases.erase(
        remove_if(Cases,
                  [PopSucc](const CaseRange &R) { return R.BB == PopSucc; }),
        Cases.end());

    // If there are no cases left, just branch.
    if (Cases.empty()) {
      BranchInst::Create(Default, CurBlock);
      SI->eraseFromParent();
      return;
    }
  }

  // Create a new, empty default block so that the new hierarchy of
  // if-then statements go to this and the PHI nodes are happy.
  BasicBlock *NewDefault = BasicBlock::Create(SI->getContext(), "NewDefault");
  F->getBasicBlockList().insert(Default->getIterator(), NewDefault);
  BranchInst::Create(Default, NewDefault);

  // If there is an entry in any PHI nodes for the default edge, make sure
  // to update them as well.
  for (BasicBlock::iterator I = Default->begin(); isa<PHINode>(I); ++I) {
    PHINode *PN = cast<PHINode>(I);
    int BlockIdx = PN->getBasicBlockIndex(OrigBlock);
    assert(BlockIdx != -1 && "Switch didn't go to this successor??");
    PN->setIncomingBlock((unsigned)BlockIdx, NewDefault);
  }

  BasicBlock *SwitchBlock =
      switchConvert(Cases.begin(), Cases.end(), LowerBound, UpperBound, Val,
                    OrigBlock, OrigBlock, NewDefault, UnreachableRanges);

  // Branch to our shiny new if-then stuff...
  BranchInst::Create(SwitchBlock, OrigBlock);

  // We are now done with the switch instruction, delete it.
  BasicBlock *OldDefault = SI->getDefaultDest();
  CurBlock->getInstList().erase(SI);

  // If the Default block has no more predecessors just add it to DeleteList.
  if (pred_begin(OldDefault) == pred_end(OldDefault))
    DeleteList.insert(OldDefault);
}

// the break-constant-expr pass
namespace {
  class BreakConstantExpr {
    public:
      bool runOnModule(Module &m);
  };
}

//
// Function: hasConstantExpr()
//
// Description:
//  This function determines whether the given value is a constant expression
//
// Inputs:
//  V - The value to check.
//
// Return value:
//  NULL  - This value is not a constant expression with a constant expression
//          GEP within it.
//  ~NULL - A pointer to the value casted into a ConstantExpr is returned.
//
static ConstantExpr *hasConstantExpr(Value *V) {
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(V)){
    return CE;
  }

  return nullptr;
}

//
// Function: convertGEP()
//
// Description:
//  Convert a GEP constant expression into a GEP instruction.
//
// Inputs:
//  CE       - The GEP constant expression.
//  InsertPt - The instruction before which to insert the new GEP instruction.
//
// Return value:
//  A pointer to the new GEP instruction is returned.
//
static Instruction *convertGEP(ConstantExpr *CE, Instruction *InsertPt) {
  // Create iterators to the indices of the constant expression.
  std::vector<Value *> Indices;
  for (unsigned index = 1; index < CE->getNumOperands(); ++index) {
    Indices.push_back (CE->getOperand (index));
  }
  ArrayRef<Value *> arrayIdices(Indices);

  // Make the new GEP instruction.
  return (GetElementPtrInst::Create(nullptr, CE->getOperand(0),
        arrayIdices, CE->getName(), InsertPt));
}

//
// Function: convertExpression()
//
// Description:
//  Convert a constant expression into an instruction.  This routine does *not*
//  perform any recursion, so the resulting instruction may have constant
//  expression operands.
//
static Instruction *convertExpression(ConstantExpr *CE, Instruction *InsertPt) {
  // Convert this constant expression into a regular instruction.
  Instruction * NewInst = 0;
  switch (CE->getOpcode()) {
    case Instruction::GetElementPtr: 
      {
        NewInst = convertGEP(CE, InsertPt);
        break;
      }

    case Instruction::Add:
    case Instruction::Sub:
    case Instruction::Mul:
    case Instruction::UDiv:
    case Instruction::SDiv:
    case Instruction::FDiv:
    case Instruction::URem:
    case Instruction::SRem:
    case Instruction::FRem:
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor: 
      {
        Instruction::BinaryOps Op = (Instruction::BinaryOps)(CE->getOpcode());
        NewInst = BinaryOperator::Create(Op,
            CE->getOperand(0), CE->getOperand(1), CE->getName(), InsertPt);
        break;
      }

    case Instruction::Trunc:
    case Instruction::ZExt:
    case Instruction::SExt:
    case Instruction::FPToUI:
    case Instruction::FPToSI:
    case Instruction::UIToFP:
    case Instruction::SIToFP:
    case Instruction::FPTrunc:
    case Instruction::FPExt:
    case Instruction::PtrToInt:
    case Instruction::IntToPtr:
    case Instruction::BitCast: 
      {
        Instruction::CastOps Op = (Instruction::CastOps)(CE->getOpcode());
        NewInst = CastInst::Create(Op,
            CE->getOperand(0), CE->getType(), CE->getName(), InsertPt);
        break;
      }

    case Instruction:: FCmp:
    case Instruction:: ICmp: 
      {
        Instruction::OtherOps Op = (Instruction::OtherOps)(CE->getOpcode());
        NewInst = CmpInst::Create(Op,
            static_cast<llvm::CmpInst::Predicate>(CE->getPredicate()),
            CE->getOperand(0), CE->getOperand(1), CE->getName(), InsertPt);
        break;
      }

    case Instruction:: Select:
      {
        NewInst = SelectInst::Create(
            CE->getOperand(0), CE->getOperand(1), CE->getOperand(2),
            CE->getName(), InsertPt);
        break;
      }

    case Instruction:: ExtractElement:
    case Instruction:: InsertElement:
    case Instruction:: ShuffleVector:
    case Instruction:: InsertValue:
    default:
        assert (0 && "Unhandled constant expression!\n");
        break;
    }

    return NewInst;
}

//
// Method: runOnModule()
//
// Description:
//  Entry point for this LLVM pass.
//
bool BreakConstantExpr::runOnModule(Module &m) {
  bool modified = false;
  
  for (Module::iterator F = m.begin(), E = m.end(); F != E; ++F) {
    // Worklist of values to check for constant GEP expressions
    std::vector<Instruction *> Worklist;

    // Initialize the worklist by finding all instructions that have one or more
    // operands containing a constant GEP expression.
    for (Function::iterator BB = (*F).begin(); BB != (*F).end(); ++BB) {
      for (BasicBlock::iterator i = BB->begin(); i != BB->end(); ++i) {
        // Scan through the operands of this instruction.  If it is a constant
        // expression GEP, insert an instruction GEP before the instruction.
        Instruction *I = &(*i);
        for (unsigned index = 0; index < I->getNumOperands(); ++index) {
          if (hasConstantExpr (I->getOperand(index))) {
            Worklist.push_back (I);
          }
        }
      }
    }

    // Determine whether we will modify anything.
    if (Worklist.size()) {
      modified = true;
    }

    // While the worklist is not empty, take an item from it, convert the
    // operands into instructions if necessary, and determine if the newly
    // added instructions need to be processed as well.
    while (Worklist.size()) {
      Instruction *I = Worklist.back();
      Worklist.pop_back();

      // Scan through the operands of this instruction and convert each into an
      // instruction.  Note that this works a little differently for phi
      // instructions because the new instruction must be added to the
      // appropriate predecessor block.
      if (PHINode *PHI = dyn_cast<PHINode>(I)) {
        for (unsigned index = 0; index < PHI->getNumIncomingValues(); ++index) {
          // For PHI Nodes, if an operand is a cons expression with a GEP, we
          // want to insert the new instructions in the predecessor basic block.
          //
          // Note: It seems that it's possible for a phi to have the same
          // incoming basic block listed multiple times; this seems okay as long
          // the same value is listed for the incoming block.
          Instruction *InsertPt = PHI->getIncomingBlock(index)->getTerminator();

          if (ConstantExpr *CE = 
              hasConstantExpr (PHI->getIncomingValue(index))) {

            Instruction *NewInst = convertExpression (CE, InsertPt);
            for (unsigned i2 = index; i2 < PHI->getNumIncomingValues(); ++i2) {
              if ((PHI->getIncomingBlock (i2)) == PHI->getIncomingBlock (index))
                PHI->setIncomingValue (i2, NewInst);

            }
            Worklist.push_back (NewInst);
          }
        }
      } else {
        for (unsigned index = 0; index < I->getNumOperands(); ++index) {
          // For other instructions, we want to insert instructions replacing
          // constant expressions immediently before the instruction using the
          // constant expression.
          if (ConstantExpr *CE = hasConstantExpr (I->getOperand(index))) {
            Instruction *NewInst = convertExpression (CE, I);
            I->replaceUsesOfWith (CE, NewInst);
            Worklist.push_back (NewInst);
          }
        }
      }
    }
  }

  return modified;
}

#include "Lower.h"

void lowerSwitch(Function &f) {
  LowerSwitch lower;
  lower.runOnFunction(f);
}

void breakConstantExpr(Module &m) {
  BreakConstantExpr lower;
  lower.runOnModule(m);
}
