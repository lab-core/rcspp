#pragma once

#include "algorithm.h"
#include "label/label_pool.h"

#include <map>
#include <memory>


template<typename ResourceType>
class DominanceAlgorithm : public Algorithm<ResourceType>{
public:

  DominanceAlgorithm(ResourceFactory<ResourceType>& resource_factory, const Graph<ResourceType>& graph, 
    std::optional<int> label_pool_size = std::nullopt) : Algorithm<ResourceType>(resource_factory, graph, label_pool_size) { }
    

private:

  void initialize_labels() override {

    std::cout << "DominanceAlgorithm::initialize_labels()\n";

    for (auto source_node_id : this->graph_.get_source_node_ids()) {
      auto& source_node = this->graph_.get_node(source_node_id);
      auto& label = this->label_pool_.get_next_label(&source_node);
      this->labels_[label.id] = &label;
    }

  }

  Label<ResourceType>& next_label() override {

    std::cout << "DominanceAlgorithm::next_label()\n";

    std::cout << "labels_.size()=" << this->labels_.size() << std::endl;

    auto next_label_it = this->labels_.begin();

    auto& next_label = *next_label_it->second;

    this->labels_.erase(next_label_it);

    std::cout << "labels_.size()=" << this->labels_.size() << std::endl;

    return next_label;
  }

  bool test(const Label<ResourceType>& label) override {

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

  void expand(Label<ResourceType>& label) override {

    std::cout << "DominanceAlgorithm::expand()\n";

    update_non_dominated_labels(label);

    auto current_node = label.get_end_node();

    for (auto arc : current_node->out_arcs) {

      auto& new_label = this->label_pool_.get_next_label();
      label.expand(*arc, new_label);
      this->labels_[new_label.id] = &new_label;
    }
  }

  void update_non_dominated_labels(Label<ResourceType>& label) {

    std::cout << "DominanceAlgorithm::update_non_dominated_labels()\n";

    auto current_node_id = label.get_end_node()->id;

    std::vector<Label<ResourceType>*> updated_non_dominated_labels;
    for (auto non_dominated_label : non_dominated_labels_by_node_id_[current_node_id]) {
      if (!(label <= *non_dominated_label)) {
        updated_non_dominated_labels.push_back(non_dominated_label);
      }
      else {
        this->labels_.erase(non_dominated_label->id);
      }
    }

    updated_non_dominated_labels.push_back(&label);

    non_dominated_labels_by_node_id_[current_node_id] = updated_non_dominated_labels;

  }

  std::map<size_t, std::vector<Label<ResourceType>*>> non_dominated_labels_by_node_id_;

};