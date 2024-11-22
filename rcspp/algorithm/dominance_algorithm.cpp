#include "dominance_algorithm.h"

#include <iostream>

DominanceAlgorithm::DominanceAlgorithm(ResourceFactory& resource_factory, const Graph& graph, 
  std::optional<int> label_pool_size) :
  Algorithm(resource_factory, graph, label_pool_size) {

}

void DominanceAlgorithm::initialize_labels() {

  std::cout << "DominanceAlgorithm::initialize_labels()\n";

  // TO DO: MODIFY TO BE ABLE TO DEAL WITH MORE THAN ONE SOURCE NODE.
  auto source_node_id = graph_.get_source_node_ids()[0];
  auto& source_node = graph_.get_node(source_node_id);


  auto& label = label_pool_.get_next_label(&source_node);

  labels_[label.id] = &label;
}

Label& DominanceAlgorithm::next_label() {

  std::cout << "DominanceAlgorithm::next_label()\n";

  std::cout << "labels_.size()=" << labels_.size() << std::endl;

  auto next_label_it = labels_.begin();

  auto& next_label = *next_label_it->second;

  labels_.erase(next_label_it);

  std::cout << "labels_.size()=" << labels_.size() << std::endl;

  return next_label;
}

bool DominanceAlgorithm::test(const Label& label) {

  std::cout << "DominanceAlgorithm::test()\n";

  bool non_dominated = true;
  for (auto non_dominated_label : non_dominated_labels_by_node_id_[label.get_end_node()->id]) {
    if ((*non_dominated_label) <= label) {
      non_dominated = false;
    }
  }

  std::cout << "non_dominated = " << non_dominated << std::endl;
    
  return non_dominated;
}

void DominanceAlgorithm::expand(Label& label) {

  std::cout << "DominanceAlgorithm::expand()\n";

  update_non_dominated_labels(label);

  auto current_node = label.get_end_node();

  for (auto arc : current_node->out_arcs) {

    auto& new_label = label_pool_.get_next_label();
    label.expand(*arc, new_label);
    labels_[new_label.id] = &new_label;
  }
}

void DominanceAlgorithm::update_non_dominated_labels(Label& label) {

  std::cout << "DominanceAlgorithm::update_non_dominated_labels()\n";

  auto current_node_id = label.get_end_node()->id;

  std::vector<Label*> updated_non_dominated_labels;
  for (auto non_dominated_label : non_dominated_labels_by_node_id_[current_node_id]) {
    if (!(label <= *non_dominated_label)) {
      updated_non_dominated_labels.push_back(non_dominated_label);
    }
    else {
      labels_.erase(non_dominated_label->id);
    }
  }

  updated_non_dominated_labels.push_back(&label);

  non_dominated_labels_by_node_id_[current_node_id] = updated_non_dominated_labels;

}
