#pragma once

#include "graph/graph.h"
#include "algorithm/algorithm.h"

#include <memory>
#include <vector>


template<typename ResourceType>
class RCSPPSolver {

public:

  RCSPPSolver(std::unique_ptr<Graph<ResourceType>> graph, std::unique_ptr<Algorithm<ResourceType>> algorithm) :
    graph_(std::move(graph)), algorithm_(std::move(algorithm)) { }

  std::vector<Solution<ResourceType>> solve() {

    std::cout << "RCSPPSolver::solve()\n";

    auto solution = algorithm_->solve();

    std::vector<Solution<ResourceType>> solutions{ solution };

    return solutions;
  }

private:
  std::unique_ptr<Graph<ResourceType>> graph_;
  std::unique_ptr<Algorithm<ResourceType>> algorithm_;

};