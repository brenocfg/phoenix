#pragma once

#include "llvm/IR/Instructions.h" // To have access to the Instructions.

#include <map>
#include <set>

using namespace llvm;

namespace phoenix {

enum DependenceType {
  DT_Data,
  DT_Control,
};

struct DependenceNode{

  Value *node;
  unsigned id;

  DependenceNode() = delete;
  DependenceNode(Value *node, unsigned id) : node(node), id(id) {}

  std::string label() {
    std::string str;
    llvm::raw_string_ostream rso(str);
    node->print(rso);
    return str;
  }

  BasicBlock* parent() {
    return cast<Instruction>(node)->getParent();
  }

  friend llvm::raw_ostream& operator<<(llvm::raw_ostream& os, DependenceNode &dn){
    // os << dn.id;
    os << *dn.node;
    return os;
  }
};

struct DNCompare {
  bool operator()(const DependenceNode *lhs, const DependenceNode *rhs) const {
    return lhs->id < rhs->id;
  }
};

using DNSet = std::set<DependenceNode*, DNCompare>;

struct DependenceEdge {
  DependenceNode *u, *v;
  DependenceType type;

  DependenceEdge() = delete;
  DependenceEdge(DependenceNode *u, DependenceNode *v, DependenceType dt)
      : u(u), v(v), type(dt) {}

  bool operator<(const DependenceEdge &other) const {
    return v < other.v;
  }

  bool operator==(const DependenceEdge &other) const {
    return v == other.v;
  }

  friend llvm::raw_ostream& operator<<(llvm::raw_ostream& os, DependenceEdge &de){
    os << *de.u << " -> " << *de.v;
    return os;
  }
};

class DependenceGraph
    : public std::map<Value *, std::set<DependenceEdge *>> {
private:
  unsigned assign_id(Value *V);
  std::map<BasicBlock*, DNSet> get_nodes();
  std::set<DependenceEdge*> get_edges();

  std::string declare_nodes();
  std::string declare_edges();

public:
  void add_edge(Value *u, Value *v, DependenceType type);

  void to_dot();

  // friend llvm::raw_ostream& operator<<(llvm::raw_ostream& os, DependenceGraph &dg){
  //   return os;
  // }

private:
  std::map<Value*, unsigned> ids;
};

} // end namespace phoenix