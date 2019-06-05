#pragma once

#include "llvm/Analysis/PostDominators.h"
#include "dependenceGraph.h"

using namespace llvm;

namespace phoenix {

class ProgramDependenceGraph {
private:
  Value *get_predicate(BasicBlock *X, BasicBlock *Y, Value *old_pred);
  void create_control_edges(BasicBlock *Y, Value *pred);
  void compute_control_dependences(DomTreeNodeBase<BasicBlock> *X, Value *pred);

  void create_data_edges(Value *start);
  void compute_data_dependences(Function *F);

public:
  std::set<Instruction *> get_all_dependences(Instruction *start);
  void compute_dependences(Function *F);

  DependenceGraph* get_dependence_graph();
  ProgramDependenceGraph(Function *F, DominatorTree *DT, PostDominatorTree *PDT);

private:
  DominatorTree *DT;
  PostDominatorTree *PDT;

  DependenceGraph *DG;
};

} // end namespace phoenix