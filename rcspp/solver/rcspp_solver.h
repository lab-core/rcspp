#pragma once

#include "graph/graph.h"
#include "algorithm/algorithm.h"

#include <memory>
#include <vector>


class RCSPPSolver {

public:

  RCSPPSolver(std::unique_ptr<Graph> graph, std::unique_ptr<Algorithm> algorithm);

  std::vector<Solution> solve();

private:
  std::unique_ptr<Graph> graph_;
  std::unique_ptr<Algorithm> algorithm_;

};