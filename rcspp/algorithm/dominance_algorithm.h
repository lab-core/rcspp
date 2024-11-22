#pragma once

#include "algorithm.h"
#include "label/label_pool.h"

#include <map>
#include <memory>


class DominanceAlgorithm : public Algorithm {
public:

  DominanceAlgorithm(ResourceFactory& resource_factory, const Graph& graph, std::optional<int> label_pool_size = std::nullopt);
    

private:

  void initialize_labels() override;

  Label& next_label() override;

  bool test(const Label& label) override;

  void expand(Label& label) override;

  void update_non_dominated_labels(Label& label);

  std::map<size_t, std::vector<Label*>> non_dominated_labels_by_node_id_;

};