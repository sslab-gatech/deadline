#include "PA.h"

using namespace SEGUtil;

void PA::summarizeFunctions(Module *module) {
}

void PA::analyzePointTo(vector<Instruction*> iList, SliceOracle &so) {
	if (iList.empty())
		llvm_unreachable("empty instruction path");

	const Function *func = iList[0]->getParent()->getParent();
	errs() << "----> " << func->getName() << '\n';
	module = func->getParent();
	dataLayout = &(module->getDataLayout());

	// initialize global variable nodes
	initializeGlobals(module);
	
	errs() << "++++++++path begin++++++++\n";
	for (Instruction *inst : iList) {
		errs() << *inst << '\n';
	}
	errs() << "++++++++ path end ++++++++\n";
	
	errs() << "=========================================\n";

	unsigned currIdx = -1;
	for (Instruction *inst : iList) {
		currIdx += 1;
		errs() << *inst << '\n';
		switch (inst->getOpcode()) {
			case Instruction::Alloca:
				{
					AllocaInst *aInst = dyn_cast<AllocaInst>(inst);
					Node *newNode = new Node(NodeType::ALLOC, InstType::INIT, aInst, aInst);
					AllNodes[aInst] = newNode;
					break;
				}
			case Instruction::Call:
			case Instruction::Invoke:
				{
					// TODO
					errs() << "TODO: Call/Invoke\n";
					break;
				}
			case Instruction::Ret:
				{
					// TODO
					errs() << "TODO: Ret\n";
					break;
				}
			case Instruction::Load:
				{
					LoadInst *lInst = dyn_cast<LoadInst>(inst);
					const Value *src = lInst->getPointerOperand();
					Node *srcNode = lookupNode(src);
					if (!srcNode) 
						llvm_unreachable("cannod find source node");
					Node *dstNode = lookupNode(lInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, lInst);
						AllNodes[lInst] = dstNode;
					}
					srcNode->getNode(srcNode->startOffset)->copy(dstNode);	
					break;
				}
			case Instruction::Store:
				{
					StoreInst *sInst = dyn_cast<StoreInst>(inst);
					const Value *src = sInst->getValueOperand();
					const Value *dst = sInst->getPointerOperand();
					Node *srcNode = lookupNode(src);
					if (!srcNode)
						llvm_unreachable("cannot find source node");
					Node *dstNode = lookupNode(dst)->getNode();
					if (!dstNode)
						llvm_unreachable("cannot find destination node");
					srcNode->copy(dstNode->getNode(dstNode->startOffset));
					break;
				}
			case Instruction::GetElementPtr:
				{
					GetElementPtrInst *gepInst = dyn_cast<GetElementPtrInst>(inst);
					// calculate the offset
					SmallVector<Value*, 4> indexOps = SmallVector<Value*, 4>(gepInst->op_begin() + 1, gepInst->op_end());
					int64_t elemOffset = 0;
					unsigned n = indexOps.size();
					for (unsigned i = 0; i < n; ++i) {
						if (!isa<ConstantInt>(indexOps[i])) {
							//llvm_unreachable("non constant index for gep inst");
							errs() << "WARN: " << "gep inst has non constant index\n";
							indexOps[i] = ConstantInt::get(Type::getInt32Ty(module->getContext()), 0);
						}
					}

					PointerType *pTy = dyn_cast<PointerType>(gepInst->getPointerOperand()->getType());
					elemOffset = dataLayout->getIndexedOffsetInType(pTy->getElementType(), indexOps);
					// get base node and element node, copy base node to dst node
					Node *dstNode = lookupNode(gepInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, gepInst);
						AllNodes[gepInst] = dstNode;
					}
					Node *baseNode = lookupNode(gepInst->getPointerOperand());
					if (!baseNode)
						llvm_unreachable("cannot find source base node");
					Node *elemNode = baseNode->getNode(elemOffset);
					if (!elemNode)
						llvm_unreachable("cannot find element node");
					baseNode->copy(dstNode);
					dstNode->startOffset = baseNode->startOffset + elemOffset;
					break;	
				}
			case Instruction::PHI:
				{
					// just pick up an existing source value, TODO: get latest source value
					PHINode *phiNode = dyn_cast<PHINode>(inst);
					Value *srcValue = SEGUtil::backtrace(currIdx, phiNode, iList, so);
					Node *dstNode = lookupNode(phiNode);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, phiNode);
						AllNodes[phiNode] = dstNode;
					}

					if (!srcValue)
						llvm_unreachable("cannot find source value for PHINode");
					Node *srcNode = lookupNode(srcValue);
					if (!srcNode)
						llvm_unreachable("cannot find PHINode source node");
					srcNode->copy(dstNode);
					
					/*
					unsigned n = phiNode->getNumIncomingValues();
					bool found = false;
					for (unsigned i = 0; i < n; ++i) {
						Node *srcNode = lookupNode(phiNode->getIncomingValue(i));
						if (srcNode) {
							srcNode->copy(dstNode);
							found = true;
							break;
						}
					}
					if (!found)
						llvm_unreachable("cannot find source value for PHINode");
					*/
					break;
				}
			case Instruction::BitCast:
				{
					BitCastInst *bcInst = dyn_cast<BitCastInst>(inst);
					Node *dstNode = lookupNode(bcInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, bcInst);
						AllNodes[bcInst] = dstNode;
					}
					Node *srcNode = lookupNode(bcInst->getOperand(0));
					if (!srcNode) 
						llvm_unreachable("cannot find source node");
					srcNode->copy(dstNode);
					break;
				}
			case Instruction::IntToPtr:
				{
					IntToPtrInst *intToPtrInst = dyn_cast<IntToPtrInst>(inst);
					// TODO
					Node *dstNode = lookupNode(intToPtrInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, intToPtrInst);
						AllNodes[intToPtrInst] = dstNode;
					}
					break;
				}
			case Instruction::PtrToInt:
				{
					PtrToIntInst *ptrToIntInst = dyn_cast<PtrToIntInst>(inst);
					// TODO
					Node *dstNode = lookupNode(ptrToIntInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, ptrToIntInst);
						AllNodes[ptrToIntInst] = dstNode;
					}
					break;
				}
			case Instruction::Select:
				{
					// randomly pick an existing value, TODO: get the latest source value
					SelectInst *sInst = dyn_cast<SelectInst>(inst);
					Node *dstNode = lookupNode(sInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, sInst);
						AllNodes[sInst] = dstNode;
					}
					Node *srcNode = lookupNode(sInst->getTrueValue());
					if (srcNode)
						srcNode->copy(dstNode);
					else {
						srcNode = lookupNode(sInst->getFalseValue());
						if (!srcNode)
							llvm_unreachable("cannot find source value for SelectInst");
						srcNode->copy(dstNode);
					}	
					break;
				}
			case Instruction::ExtractElement:
				{
					ExtractElementInst *extractElementInst = dyn_cast<ExtractElementInst>(inst);
					const Value *indexOperand = extractElementInst->getIndexOperand();
					if (!isa<ConstantInt>(indexOperand)) {
						//llvm_unreachable("extract element inst has non constant index");
						errs() << "WARN: " << "extract element inst has non constant index\n";
						indexOperand = ConstantInt::get(Type::getInt32Ty(module->getContext()), 0);
					}
					const ConstantInt *cInt = dyn_cast<ConstantInt>(indexOperand);
					int64_t elemOffset = cInt->getSExtValue();
					Node *dstNode = lookupNode(extractElementInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, extractElementInst);
						AllNodes[extractElementInst] = dstNode;
					}
					Node *vectorNode = lookupNode(extractElementInst->getVectorOperand());
					if (!vectorNode) 
						llvm_unreachable("cannot find source vector node");
					Node *elemNode = vectorNode->getNode(elemOffset);
					elemNode->copy(dstNode);
					break;
				}
			case Instruction::ExtractValue:
				{
					ExtractValueInst *extractValueInst = dyn_cast<ExtractValueInst>(inst);
					SmallVector<Value*, 4> indexOps = SmallVector<Value*, 4>(extractValueInst->op_begin() + 1, extractValueInst->op_end());
					unsigned n = indexOps.size();
					for (unsigned i = 0; i < n; ++i) {
						if (!isa<ConstantInt>(indexOps[i])) {
							//llvm_unreachable("extract value inst has non constant index");
							errs() << "WARN: " << "extract value inst has non constant index\n";
							indexOps[i] = ConstantInt::get(Type::getInt32Ty(module->getContext()), 0);
						}
					}
					PointerType *ptrTy = PointerType::get(extractValueInst->getAggregateOperand()->getType(), 0);
					int64_t elemOffset = dataLayout->getIndexedOffsetInType(ptrTy->getElementType(), indexOps);
					Node *dstNode = lookupNode(extractValueInst);
					if (!dstNode) {
						dstNode = new Node(NodeType::UNDEFINED, InstType::READ, nullptr, extractValueInst);
						AllNodes[extractValueInst] = dstNode;
					}
					Node *baseNode = lookupNode(extractValueInst->getAggregateOperand());
					if (!baseNode)
						llvm_unreachable("cannot find base node");
					Node *elemNode = baseNode->getNode(elemOffset);
					if (!elemNode)
						llvm_unreachable("cannot find element node");
					elemNode->copy(dstNode);
					break;
				}
			case Instruction::InsertElement:
				{
					InsertElementInst *insertElementInst = dyn_cast<InsertElementInst>(inst);
					const Value *indexOperand = insertElementInst->getOperand(2);
					if (!isa<ConstantInt>(indexOperand)) {
						//llvm_unreachable("insert element inst has non constant index");
						errs() << "WARN: " << "insert element inst has non constant index\n";
						indexOperand = ConstantInt::get(Type::getInt32Ty(module->getContext()), 0);
					}
					const ConstantInt *cInt = dyn_cast<ConstantInt>(indexOperand);
					int64_t elemOffset = cInt->getSExtValue();
					Node *baseNode = lookupNode(insertElementInst->getOperand(0));
					if (!baseNode)
						llvm_unreachable("cannot find base node");
					Node *elemNode = baseNode->getNode(elemOffset);
					if (!elemNode)
						llvm_unreachable("cannot find element node");
					Node *srcNode = lookupNode(insertElementInst->getOperand(1));
					if (!srcNode)
						llvm_unreachable("cannot find source node");
					srcNode->copy(elemNode);
					break;
				}
			case Instruction::InsertValue:
				{
					InsertValueInst *insertValueInst = dyn_cast<InsertValueInst>(inst);
					SmallVector<Value*, 4> indexOps = SmallVector<Value*, 4>(insertValueInst->op_begin() + 2, insertValueInst->op_end());
					for (unsigned i = 0; i < indexOps.size(); ++i) {
						if (!isa<ConstantInt>(indexOps[i])) {
							//llvm_unreachable("extract value inst has non constant index");
							errs() << "WARN: " << "extract value inst has non constant index\n";
							indexOps[i] = ConstantInt::get(Type::getInt32Ty(module->getContext()), 0);
						}
					}
					PointerType *ptrTy = PointerType::get(insertValueInst->getAggregateOperand()->getType(), 0);
					int64_t elemOffset = dataLayout->getIndexedOffsetInType(ptrTy->getElementType(), indexOps);
					Node *baseNode = lookupNode(insertValueInst->getAggregateOperand());
					if (!baseNode)
						llvm_unreachable("cannot find base node");
					Node *elemNode = baseNode->getNode(elemOffset);
					if (!elemNode)
						llvm_unreachable("cannot find element node");
					Node *srcNode = lookupNode(insertValueInst->getInsertedValueOperand());
					if (!srcNode)
						llvm_unreachable("cannot find source node");
					srcNode->copy(elemNode);
					break;
				}
			default:
				{
					errs() << "WARN: unhandled instruction - " << *inst << '\n';
					Node *newNode = new Node(NodeType::ALLOC, InstType::INIT, inst, inst);
					AllNodes[inst] = newNode;
					break;
				}
		}
	}
}
