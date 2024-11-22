#pragma once

#include "label/label_pool.h"
#include "graph/graph.h"
#include "solution.h"

#include <vector>
#include <memory>


class Algorithm {
public:

  Algorithm(ResourceFactory& resource_factory, const Graph& graph, std::optional<int> label_pool_size = std::nullopt);
  
  virtual Solution solve();

protected:

  virtual void initialize_labels() = 0;

  virtual Label& next_label() = 0;

  virtual bool test(const Label& label) = 0;

  virtual void expand(Label& label) = 0;

  LabelPool label_pool_;

  const Graph& graph_;

  std::map<size_t, Label*> labels_;

  double cost_upper_bound_;

  Label* best_label_;
};