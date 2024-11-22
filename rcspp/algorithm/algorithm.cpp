#include "algorithm.h"
#include "label/label_pool.h"

#include <cassert>
#include <iostream>

Algorithm::Algorithm(ResourceFactory& resource_factory, const Graph& graph, std::optional<int> label_pool_size) : 
  label_pool_(LabelPool(std::make_unique<LabelFactory>(resource_factory), label_pool_size)), graph_(graph), 
  cost_upper_bound_(std::numeric_limits<double>::infinity()), best_label_(nullptr) {

}

Solution Algorithm::solve() {

  std::cout << "Algorithm::solve()\n";

  initialize_labels();

  while (labels_.size() > 0) {

    auto& label = next_label();

    std::cout << "label.id=" << label.id << " | " << label.get_end_node()->id << std::endl;

    assert(label.get_end_node());

    std::cout << "graph_.is_sink(label.get_end_node()->id)=" << graph_.is_sink(label.get_end_node()->id) << std::endl;
    std::cout << "label.is_feasible()=" << label.is_feasible() << std::endl;
    std::cout << "label.get_cost() < cost_upper_bound_=" << (label.get_cost() < cost_upper_bound_) << std::endl;

    if (graph_.is_sink(label.get_end_node()->id)
      && label.is_feasible() 
      && (label.get_cost() < cost_upper_bound_)) {

      std::cout << "NEW BEST LABEL!\n";
      
      cost_upper_bound_ = label.get_cost();
      best_label_ = &label;
    }

    if (test(label)) {

      expand(label);
    }
  }

  return Solution{best_label_, cost_upper_bound_};
}