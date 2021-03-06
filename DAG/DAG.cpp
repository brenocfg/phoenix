#include "llvm/ADT/Statistic.h"  // For the STATISTIC macro.
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/VectorUtils.h"
#include "llvm/IR/Constants.h"          // For ConstantData, for instance.
#include "llvm/IR/DebugInfoMetadata.h"  // For DILocation
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"  // To use the iterator instructions(f)
#include "llvm/IR/Instructions.h"  // To have access to the Instructions.
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"  // for command line opts
#include "llvm/Support/Debug.h"        // To print error messages.
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"  // For dbgs()
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <queue>
#include <tuple>

#include "DAG.h"
#include "ReachableNodes.h"
#include "intra_profile.h"
#include "depthVisitor.h"
#include "dotVisitor.h"
#include "insertIf.h"
#include "inter_profile.h"
#include "propagateAnalysisVisitor.h"

#define DEBUG_TYPE "DAG"

enum OptType {
  LoadElimination,
  IntraProfilling,
  InterProfilling,
  StoreElimination,
};

cl::opt<OptType> DagInstrumentation(
    "dag-opt",
    cl::desc("Type of instrumentation"),
    cl::init(OptType::StoreElimination),
    cl::values(clEnumValN(OptType::LoadElimination, "eae", "no profilling at all"),
               clEnumValN(OptType::IntraProfilling, "alp", "Inner loop profile"),
               clEnumValN(OptType::StoreElimination, "ess", "just check if the store is silent"),
               clEnumValN(OptType::InterProfilling, "plp", "Outer loop profiler!")));

// This should implement a cost model
// Right now we only insert the `if` if the depth is >= threshold(1)
// TO-DO: Use a more sophisticated solution
bool DAG::worth_insert_if(Geps &g, unsigned loop_threshold = 1) {
  if (g.get_loop_depth() >= loop_threshold)
    return true;

  DEBUG(dbgs() << "skipping: " << *g.get_instruction() << "\n"
               << " threshold " << g.get_loop_depth() << " is not greater than " << loop_threshold
               << "\n\n");
  return false;
}

void DAG::update_passes(BasicBlock *from, BasicBlock *to) {
  // update LoopInfo
  Loop *L = this->LI->getLoopFor(from);
  L->addBasicBlockToLoop(to, *this->LI);

  this->DT->insertEdge(from, to);
  
  BranchInst *term = cast<BranchInst>(to->getTerminator());
  for (unsigned i = 0; i < term->getNumSuccessors(); ++i){
    BasicBlock *old_to = term->getSuccessor(i);
    
    assert(old_to != nullptr && "old_to is nullptr");
    
    // update Dominator Tree
    this->DT->deleteEdge(from, old_to);
    this->DT->insertEdge(to, old_to);
  }
}

void DAG::split(StoreInst *store) {
  BasicBlock *from = store->getParent();
  BasicBlock *to = from->splitBasicBlock(store->getNextNode());
  to->setName("split");
  
  update_passes(from, to);
}

void DAG::split(BasicBlock *from) {
  if (isa<PHINode>(from->begin())) {
    Instruction *I = from->getFirstNonPHI();
    BasicBlock *to = from->splitBasicBlock(I);
    to->setName("split");

    update_passes(from, to);
  }
}

//
void DAG::run_dag_opt(Function &F) {
  auto geps = this->Idtf->get_instructions_of_interest();

  if (geps.size() == 0)
    return;

  std::vector<ReachableNodes> reachables;

  for (auto &g : geps) {
    Instruction *I = g.get_instruction();

    // sanity check for vector instructions
    if (I->getOperand(0)->getType()->isVectorTy() || I->getOperand(1)->getType()->isVectorTy()) {
      continue;
    }

    if (!worth_insert_if(g))
      continue;

    // Split the basic block after each store instruction
    split(g.get_store_inst());
    split(g.get_store_inst()->getParent());

    phoenix::StoreNode *store =
        cast<phoenix::StoreNode>(myParser(g.get_store_inst(), g.get_operand_pos()));

    propagateAnalysisVisitor cv(store, &g);
    DepthVisitor dv(store);

    NodeSet s = dv.getSet();

    // DotVisitor dot(store);
    // dot.print();

    reachables.push_back(
        ReachableNodes(g.get_store_inst(), g.get_load_inst(), g.get_instruction(), s));
  }

  switch (DagInstrumentation) {
    case OptType::InterProfilling:
      phoenix::inter_profilling(&F, this->LI, this->DT, reachables);
      break;
    case OptType::IntraProfilling:
      phoenix::intra_profilling(&F, reachables);
      break;
    case OptType::LoadElimination:
      phoenix::load_elimination(&F, reachables);
      break;
    default:
      phoenix::silent_store_elimination(&F, reachables);
  }
}

//
bool DAG::runOnFunction(Function &F) {
  if (F.isDeclaration() || F.isIntrinsic() || F.hasPrivateLinkage() || F.hasAvailableExternallyLinkage())
    return false;

  Idtf = &getAnalysis<Identify>();
  LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  run_dag_opt(F);

  return true;
}

//
void DAG::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LoopInfoWrapperPass>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.addRequired<PostDominatorTreeWrapperPass>();
  AU.addRequired<Identify>();
}

char DAG::ID = 0;
static RegisterPass<DAG> X("DAG", "DAG pattern a = a OP b", false, false);
