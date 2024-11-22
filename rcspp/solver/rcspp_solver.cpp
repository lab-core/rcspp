#include "rcspp_solver.h"

#include <iostream>


RCSPPSolver::RCSPPSolver(std::unique_ptr<Graph> graph, std::unique_ptr<Algorithm> algorithm) : 
  graph_(std::move(graph)), algorithm_(std::move(algorithm)) {

}

std::vector<Solution> RCSPPSolver::solve() {

  std::cout << "RCSPPSolver::solve()\n";

  auto solution = algorithm_->solve();

  std::vector<Solution> solutions{ solution };

  return solutions;
}