#include "PA.h"

void PA::initializeGlobals(const Module *module) {
	// global variables
	for (auto const& globalVal : module->globals()) {
		Node *newNode = new Node(NodeType::GLOBAL, InstType::INIT, &globalVal, nullptr);
		AllNodes[&globalVal] = newNode;
	}

	// functions
	for (auto const& f : *module) {
		Node *newNode = new Node(NodeType::GLOBAL, InstType::INIT, &f, nullptr);
		(*(newNode->mem))[0] = new Node(NodeType::GLOBAL, InstType::NONE, &f, nullptr);
		AllNodes[&f] = newNode;

		// create nodes for arguments
		Function::const_arg_iterator fItr = f.arg_begin();
		while (fItr != f.arg_end()) {
			const Argument *arg = &(*fItr);
			Node *newNode = new Node(NodeType::ALLOC, InstType::INIT, arg, nullptr);
			AllNodes[arg] = newNode;

			++fItr;
		}
	}	

	// global alias
	for (auto const& globalAlias : module->aliases()) {
		Node *gNode = lookupNode(globalAlias.getBaseObject());
		AllNodes[&globalAlias] = gNode;
	}

	// initialize globals
	for (auto const& globalVal : module->globals()) {
		if (globalVal.hasDefinitiveInitializer()) {
			SmallVector<Value *, 4> indexOps;
			IntegerType * Int32Ty = IntegerType::getInt32Ty(module->getContext());
			ConstantInt * CI = ConstantInt::get(Int32Ty, 0);
			indexOps.push_back(CI);
			initGlobal(&globalVal, globalVal.getInitializer(), indexOps);
		}
	}
}

Node* PA::getNodeForConstantExpr(Value *v) {
	if (GlobalValue *gVal = dyn_cast<GlobalValue>(v)) {
		Node *gNode = getGlobalNode(v);
		if (!gNode)
			llvm_unreachable("cannot get node for global value");
		return gNode;
	}

	ConstantExpr *ce = dyn_cast<ConstantExpr>(v);
	if (!ce) {
		llvm_unreachable("not constant expr");
	}
	switch (ce->getOpcode()) {
		case Instruction::GetElementPtr:
			{
				Node *gNode = nullptr;

				GlobalVariable *gVal = dyn_cast<GlobalVariable>(ce->getOperand(0));
				if (gVal) {
					gNode = getGlobalNode(gVal);
				} else  {
					gNode = getNodeForConstantExpr(ce->getOperand(0));
				}

				if (!gNode)
					llvm_unreachable("cannot get node for gep");

				SmallVector<Value*, 4> indexOps(ce->op_begin() + 1, ce->op_end());
				PointerType *pTy = dyn_cast<PointerType>(ce->getOperand(0)->getType());
				if (!pTy)
					llvm_unreachable("not a pointer type");
				int64_t elemOffset = dataLayout->getIndexedOffsetInType(pTy->getElementType(), indexOps);
				// elemNode = gNode->mem[elemOffset]

				Node *newNode = new Node(NodeType::GLOBAL, InstType::INIT, nullptr, nullptr);
				newNode->mem = gNode->mem;
				newNode->startOffset = gNode->startOffset + elemOffset;

				Node *elemNode = gNode->getNode(elemOffset);
				if (elemNode == nullptr) {
					elemNode = new Node(NodeType::UNDEFINED, InstType::INIT, nullptr, nullptr);
					gNode->insertNode(elemOffset, elemNode);
				}

				return newNode;

			}
		case Instruction::BitCast:
			{
					Node *gNode = nullptr;
					GlobalVariable *gVal = dyn_cast<GlobalVariable>(ce->getOperand(0));
					if (gVal) {
						gNode = getGlobalNode(gVal);
					} else {
						gNode = getNodeForConstantExpr(ce->getOperand(0));
					}
					return gNode;
			}
		case Instruction::PtrToInt:
        case Instruction::IntToPtr:
			{
				Node *newNode = new Node(NodeType::UNDEFINED, InstType::INIT, nullptr, nullptr);
				return newNode;
			}
		default:
			llvm_unreachable("not getelementptr/bitcast");
			break;
	}
	llvm_unreachable("end of getNodeForConstantExpr");
	return nullptr;
}

void PA::initGlobal(const Value *V, const Constant *C, 
		SmallVector<Value *, 4>& indexOps) {
	int64_t offset = 0;
	if (const BlockAddress *bAddr = dyn_cast<BlockAddress>(C)) {
		llvm_unreachable("unhandled global block address");
	}	else if (const ConstantAggregateZero *caz = dyn_cast<ConstantAggregateZero>(C)) {
		// don't need to initialize zero initializer
		return;
	} else if (const ConstantArray *ca = dyn_cast<ConstantArray>(C)) {
		unsigned opNum = ca->getNumOperands();
		for (unsigned i = 0; i < opNum; ++i) {
			IntegerType * Int32Ty = IntegerType::getInt32Ty(module->getContext());
			ConstantInt * CI = ConstantInt::get(Int32Ty, i);
			indexOps.push_back(CI);
			initGlobal(V, ca->getOperand(i), indexOps);
			indexOps.pop_back();
		}
		return;
	} else if (const ConstantDataSequential *cds = dyn_cast<ConstantDataSequential>(C)) {
		unsigned elemNum = cds->getNumElements();
		for (unsigned i = 0; i < elemNum; ++i) {
			IntegerType * Int32Ty = IntegerType::getInt32Ty(module->getContext());
			ConstantInt * CI = ConstantInt::get(Int32Ty, i);
			indexOps.push_back(CI);
			initGlobal(V, cds->getElementAsConstant(i), indexOps);
			indexOps.pop_back();
		}
		return;
	} else if (const ConstantExpr *ce = dyn_cast<ConstantExpr>(C) ) {
		PointerType *pTy = dyn_cast<PointerType>(V->getType());
		if (!pTy)
			llvm_unreachable("not pointer type");
		offset = dataLayout->getIndexedOffsetInType(pTy->getElementType(), indexOps);
		switch (ce->getOpcode()) {
			case Instruction::GetElementPtr:
				{
					Node *elemNode = getNodeForConstantExpr(const_cast<ConstantExpr*>(ce));
					addGlobalNode(V, offset, elemNode);
					break;
				}
			case Instruction::BitCast:
				{
					Node *gNode = getNodeForConstantExpr(const_cast<ConstantExpr*>(ce));
					addGlobalNode(V, offset, gNode);
					break;
				}
			case Instruction::PtrToInt:
			case Instruction::IntToPtr:
				{
					Node *gNode = getNodeForConstantExpr(const_cast<ConstantExpr*>(ce));
					addGlobalNode(V, offset, gNode);
					break;
				}
			default:
				llvm_unreachable("unhandled constant expession in global variable initialization");
		}
		return;
	} else if (const ConstantFP *cfp = dyn_cast<ConstantFP>(C)) {
		// don't need to initialize float pointing number
		return;
	} else if (const ConstantInt *ci = dyn_cast<ConstantInt>(C)) {
		// don't need to initialize int
		return;
	} else if (const ConstantPointerNull *cpn = dyn_cast<ConstantPointerNull>(C)) {
		// intialize to undefined node
		PointerType *pTy = dyn_cast<PointerType>(V->getType());
		if (!pTy)
			llvm_unreachable("not a pointer type");
		offset = dataLayout->getIndexedOffsetInType(pTy->getElementType(), indexOps);
		addGlobalNode(V, offset, new Node(NodeType::UNDEFINED, InstType::INIT, nullptr, nullptr));
		return;
	} else if (const ConstantStruct *cs = dyn_cast<ConstantStruct>(C)) {
		unsigned opNum = cs->getNumOperands();
		for (unsigned i = 0; i < opNum; ++i) {
			IntegerType * Int32Ty = IntegerType::getInt32Ty(module->getContext());
			ConstantInt * CI = ConstantInt::get(Int32Ty, i);
			indexOps.push_back(CI);
			initGlobal(V, cs->getOperand(i), indexOps);
			indexOps.pop_back();
		}
		return;
	} else if (const ConstantVector *cv = dyn_cast<ConstantVector>(C)) {
		unsigned opNum = cv->getNumOperands();
		for (unsigned i = 0; i < opNum; ++i) {
			IntegerType * Int32Ty = IntegerType::getInt32Ty(module->getContext());
			ConstantInt * CI = ConstantInt::get(Int32Ty, i);
			indexOps.push_back(CI);
			initGlobal(V, cs->getOperand(i), indexOps);
			indexOps.pop_back();
		}
		return;
	} else if (const GlobalValue *gv = dyn_cast<GlobalValue>(C)) {
		// add the node
		Node *gNode = getGlobalNode(gv);
		// GlobalNodes[V]->mem[offset] = gNode
		PointerType *pTy = dyn_cast<PointerType>(V->getType());
		if (!pTy)
			llvm_unreachable("not a pointer type");
		offset = dataLayout->getIndexedOffsetInType(pTy->getElementType(), indexOps);
		addGlobalNode(V, offset, gNode);
		return;
	} else if (const UndefValue *uv = dyn_cast<UndefValue>(C)) {
		// intialize to undefined node
		PointerType *pTy = dyn_cast<PointerType>(V->getType());
		if (!pTy)
			llvm_unreachable("not a pointer type");
		offset = dataLayout->getIndexedOffsetInType(pTy->getElementType(), indexOps);
		addGlobalNode(V, offset, new Node(NodeType::UNDEFINED, InstType::INIT, nullptr, nullptr));
		return;
	}

	llvm_unreachable("not handled global initialization");
	return;
}

int64_t PA::initGlobal(const Value *V, const Constant *C, int64_t offset) {
	if (const BlockAddress *bAddr = dyn_cast<BlockAddress>(C)) {
		llvm_unreachable("unhandled global block address");
	}	else if (const ConstantAggregateZero *caz = dyn_cast<ConstantAggregateZero>(C)) {
		// don't need to initialize zero initializer
		int64_t tySize = dataLayout->getTypeAllocSize(caz->getType());
		// update offset
		return (offset + tySize);
	} else if (const ConstantArray *ca = dyn_cast<ConstantArray>(C)) {
		unsigned opNum = ca->getNumOperands();
		for (unsigned i = 0; i < opNum; ++i) {
			// update offset
			offset = initGlobal(V, ca->getOperand(i), offset);
		}
		return offset;
	} else if (const ConstantDataSequential *cds = dyn_cast<ConstantDataSequential>(C)) {
		unsigned elemNum = cds->getNumElements();
		for (unsigned i = 0; i < elemNum; ++i) {
			// update offset
			offset = initGlobal(V, cds->getElementAsConstant(i), offset);
		}
		return offset;
	} else if (const ConstantExpr *ce = dyn_cast<ConstantExpr>(C) ) {
		switch (ce->getOpcode()) {
			case Instruction::GetElementPtr:
				{
					Node *elemNode = getNodeForConstantExpr(const_cast<ConstantExpr*>(ce));
					addGlobalNode(V, offset, elemNode);
					// update offset
					offset += dataLayout->getTypeAllocSize(ce->getType());
					break;
				}
			case Instruction::BitCast:
				{
					Node *gNode = getNodeForConstantExpr(const_cast<ConstantExpr*>(ce));
					addGlobalNode(V, offset, gNode);
					// update offset
					offset += dataLayout->getTypeAllocSize(ce->getType());
					break;
				}
			case Instruction::PtrToInt:
				{
					Node *gNode = getNodeForConstantExpr(const_cast<ConstantExpr*>(ce));
					addGlobalNode(V, offset, gNode);
					// update offset
					offset += dataLayout->getTypeAllocSize(ce->getType());
					break;
				}
			default:
				llvm_unreachable("unhandled constant expession in global variable initialization");
		}
		return offset;
	} else if (const ConstantFP *cfp = dyn_cast<ConstantFP>(C)) {
		// don't need to initialize float pointing number
		int64_t tySize = dataLayout->getTypeAllocSize(cfp->getType());
		// update offset
		return (offset + tySize);
	} else if (const ConstantInt *ci = dyn_cast<ConstantInt>(C)) {
		// don't need to initialize int
		int64_t tySize = dataLayout->getTypeAllocSize(ci->getType());
		// update offset
		return (offset + tySize);
	} else if (const ConstantPointerNull *cpn = dyn_cast<ConstantPointerNull>(C)) {
		// intialize to undefined node
		addGlobalNode(V, offset, new Node(NodeType::UNDEFINED, InstType::INIT, nullptr, nullptr));
		int64_t tySize = dataLayout->getTypeAllocSize(cpn->getType());
		// update offset
		return (offset + tySize);
	} else if (const ConstantStruct *cs = dyn_cast<ConstantStruct>(C)) {
		unsigned opNum = cs->getNumOperands();
		for (unsigned i = 0; i < opNum; ++i) {
			offset = initGlobal(V, cs->getOperand(i), offset);
		}
		return offset;
	} else if (const ConstantVector *cv = dyn_cast<ConstantVector>(C)) {
		unsigned opNum = cv->getNumOperands();
		for (unsigned i = 0; i < opNum; ++i) {
			offset = initGlobal(V, cv->getOperand(i), offset);
		}
		return offset;
	} else if (const GlobalValue *gv = dyn_cast<GlobalValue>(C)) {
		// add the node
		Node *gNode = getGlobalNode(gv);
		// GlobalNodes[V]->mem[offset] = gNode
		addGlobalNode(V, offset, gNode);
		// update offset
		int64_t tySize = dataLayout->getTypeAllocSize(gv->getType());
		return (offset + tySize);
	} else if (const UndefValue *uv = dyn_cast<UndefValue>(C)) {
		// intialize to undefined node
		addGlobalNode(V, offset, new Node(NodeType::UNDEFINED, InstType::INIT, nullptr, nullptr));
		int64_t tySize = dataLayout->getTypeAllocSize(uv->getType());
		// update offset
		return (offset + tySize);
	}

	llvm_unreachable("not handled global initialization");
	return 0;
}
