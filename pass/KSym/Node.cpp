#include "Node.h"

 Node* Node::GLOBAL_CONSTANT_NODE = new Node(NodeType::UNDEFINED, InstType::NONE, nullptr, nullptr);

// copy this's mem to dstNode's mem
void Node::copy(Node *dstNode) {
	dstNode->nType = this->nType;
	dstNode->value = this->value;
	dstNode->mem = this->mem;
	dstNode->startOffset = this->startOffset;
}

void Node::deepCopy(Node * dstNode) {
	dstNode->nType = this->nType;
	dstNode->value = this->value;
	dstNode->startOffset = this->startOffset;

	for (auto iter = this->mem->begin(); iter != this->mem->end(); iter++)
		(*dstNode->mem)[iter->first] = iter->second;
}

Node* Node::getNode(int64_t offset) {
	int64_t realOffset = startOffset + offset;

	if (realOffset < 0) {
		errs() << startOffset << ' ' << offset << '\n';
		llvm_unreachable("wrong offset");
	}

	unordered_map<int64_t, Node*>::iterator ret = mem->find(realOffset);
	Node *retNode = nullptr;

	if (ret == mem->end()) {
		Node *newNode = new Node(NodeType::UNDEFINED, InstType::NONE, nullptr, nullptr);

		pair<int64_t, Node*> p (realOffset, newNode);
		mem->insert(p);

		retNode = newNode;
	} else {
		retNode = ret->second;
	}

	if (!retNode)
		llvm_unreachable("return null node");

	return retNode;
}

Node* Node::getNode() {
	return getNode(0);
}

void Node::insertNode(Node *node) {
	insertNode(0, node);
}

void Node::insertNode(int64_t offset, Node *node) {
	if (!node)
		llvm_unreachable("insert null node\n");

	int64_t realOffset = startOffset + offset;

	if (realOffset < 0)
		llvm_unreachable("wrong offset");

	if (!node)
		llvm_unreachable("insert null node");

	auto it = mem->find(realOffset);
	if (it != mem->end()) {
		it->second = node;
	} else {
		pair<int64_t, Node*> p (realOffset, node);
		mem->insert(p);
	}
}

void Node::dump() {
	std::unordered_set<Node *> visited;
	dump(visited, 0);
	visited.clear();
}

void Node::dump(std::unordered_set<Node *> &visited, unsigned indent) 
{
	std::error_code EC;
	raw_fd_ostream * ofs = new raw_fd_ostream("log.txt", EC, sys::fs::F_RW | sys::fs::F_Text);
	if (visited.find(this) != visited.end())
		return;
	else
		visited.insert(this);

	ofs->indent(indent) << "*********this@" << this << "\n";
	if (const Function * F = dyn_cast_or_null<Function>(value))
		ofs->indent(indent) << "v: @" << F->getName() << "\n";
	else if (value)
		ofs->indent(indent) << "v: " << *value << '\n';
	else
		ofs->indent(indent) << "v: null\n";

	ofs->indent(indent) << "startOffset: " << startOffset << '\n';

	if (!mem) {
		ofs->indent(indent) << "mem is nullptr\n";
		//llvm_unreachable("here");
	} else {
		ofs->indent(indent) << "mem size: " << mem->size() << " @ " << mem << '\n';
		unordered_map<int64_t, Node*>::iterator it = mem->begin(), ie = mem->end();
		for (; it != ie; ++it) {
			ofs->indent(indent) << "offset: " << it->first << '\n';
			Node *subNode = it->second;
			if (subNode) {
				ofs->indent(indent) <<"subnode@" << subNode << '\n';

				subNode->dump(visited, indent + 1);
			} else
				llvm_unreachable("null node\n");
		}
	}
}
