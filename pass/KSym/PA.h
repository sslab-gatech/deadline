#ifndef __PA__H
#define __PA__H

#include "Node.h"
#include "Project.h"

typedef DenseMap<const Value*, Node*> Value2NodeMap;

class PA
{
	public:
		Value2NodeMap AllNodes;
		const Module *module;
		const DataLayout *dataLayout;

		Node* lookupNode(const Value *value) {
			if (const ConstantData *cData = dyn_cast<ConstantData>(value)) {
				return Node::GLOBAL_CONSTANT_NODE;
			}
			Value2NodeMap::iterator it = AllNodes.find(value);
			if (it == AllNodes.end())
				return nullptr;
			return it->second;
		}

		~PA() {
			for (auto it : AllNodes) {
				Node *node = it.second;
				delete node;
			}
			AllNodes.clear();
		}

		void summarizeFunctions(Module *module);

		void addGlobalNode(const Value *gVal, uint64_t offset, Node *node) {
			DenseMap<const Value*, Node*>::iterator it = AllNodes.find(gVal);
			if (it == AllNodes.end()) {
				llvm_unreachable("cannot find global node");
			}
			it->second->insertNode(offset, node);
		}

		Node* getGlobalNode(const Value *gVal) {
			DenseMap<const Value*, Node*>::iterator it = AllNodes.find(gVal);
			if (it == AllNodes.end()) {
				llvm_unreachable("cannot find global node");
			}
			return it->second;
		}

	Node* getNodeForConstantExpr(Value *v);
	void initGlobal(const Value *V, const Constant *C, SmallVector<Value *, 4>& indexOps);
	int64_t initGlobal(const Value *V, const Constant *C, int64_t offset);
	void initializeGlobals(const Module *module);
	void analyzePointTo(vector<Instruction*> iList, SliceOracle &so);
};

#endif
