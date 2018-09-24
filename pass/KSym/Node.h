#ifndef __NODE__H
#define __NODE__H

#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

using namespace llvm;
using namespace std;

class Node;

// four node types:
// 	- GLOBAL: global variables, functions
// 	- ALLOC: local variables allocated in functions
// 	- HEAP: objects allocated by calling malloc/calloc
// 	- UNDEFINED: the node doesn't point to any value
enum NodeType
{
	GLOBAL,
	ALLOC,
	HEAP,
	UNDEFINED,
};

// instructions may affect the memory: 
// - WRITE: the instruction will write the memory
// - READ: the instruction will read the memory
enum InstType
{
	INIT,
	WRITE,
	READ,
	NONE,
};

class Node
{
public:
	NodeType nType;
	InstType iType;
	// the value represent the memory, a = allocate ..., b = malloc (...)
	const Value *value = nullptr;
	unordered_map<int64_t, Node*> *mem = nullptr;
	// save the 'mem' allocated by this node, deconstructor can delete the 'mem'
	unordered_map<int64_t, Node*> *savedMem = nullptr;
	int64_t startOffset = 0;

	// the instruction/argument may touch the memory
	const Value *inst = nullptr;

	static Node *GLOBAL_CONSTANT_NODE;

public:
	Node(NodeType nType_, InstType iType_, const Value *value_ = nullptr, const Value *inst_ = nullptr): 
		nType (nType_), iType (iType_), value (value_), inst (inst_) {
			mem = new unordered_map<int64_t, Node*> ();
			savedMem = mem;
		}

	~Node() {
		delete savedMem;
	}

	Node* getNode();
	Node* getNode(int64_t offset);

	void insertNode(int64_t offset, Node *node);
	void insertNode(Node *node);

	// copy this's mem to dstNode's mem
	void copy(Node *dstNode);
	void deepCopy(Node * dstNode);

	void dump();
	void dump(std::unordered_set<Node *> &, unsigned);
};

#endif
