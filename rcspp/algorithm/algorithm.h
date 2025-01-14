#pragma once

#include "label/label_pool.h"
#include "graph/graph.h"
#include "solution.h"

#include <vector>
#include <memory>
#include <cassert>


template<typename ResourceType>
class Algorithm {
public:

  Algorithm(ResourceFactory<ResourceType>& resource_factory, const Graph<ResourceType>& graph, 
    std::optional<int> label_pool_size = std::nullopt) :
    label_pool_(LabelPool<ResourceType>(std::make_unique<LabelFactory<ResourceType>>(resource_factory), label_pool_size)), graph_(graph),
    cost_upper_bound_(std::numeric_limits<double>::infinity()), best_label_(nullptr) { }
  
  virtual Solution<ResourceType> solve() {

    std::cout << "Algorithm::solve()\n";

    initialize_labels();

    while (labels_.size() > 0) {

      auto& label = next_label();

      //std::cout << "label.id=" << label.id << " | " << label.get_end_node()->id << std::endl;

      assert(label.get_end_node());

      //std::cout << "graph_.is_sink(label.get_end_node()->id)=" << graph_.is_sink(label.get_end_node()->id) << std::endl;
      //std::cout << "label.is_feasible()=" << label.is_feasible() << std::endl;
      //std::cout << "label.get_cost() < cost_upper_bound_=" << (label.get_cost() < cost_upper_bound_) << std::endl;

      if (graph_.is_sink(label.get_end_node()->id)
        && label.is_feasible()
        && (label.get_cost() < cost_upper_bound_)) {

        //std::cout << "NEW BEST LABEL!\n";

        cost_upper_bound_ = label.get_cost();
        best_label_ = &label;
      }
      else if (test(label)) {

        expand(label);
      }
    }

    return Solution<ResourceType>{ best_label_, cost_upper_bound_ };
  }

protected:

  virtual void initialize_labels() = 0;

  virtual Label<ResourceType>& next_label() = 0;

  virtual bool test(const Label<ResourceType>& label) = 0;

  virtual void expand(Label<ResourceType>& label) = 0;

  LabelPool<ResourceType> label_pool_;

  const Graph<ResourceType>& graph_;

  std::map<size_t, Label<ResourceType>*> labels_;

  double cost_upper_bound_;

  Label<ResourceType>* best_label_;
};