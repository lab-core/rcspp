// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>

#include "rcspp/algorithm/dominance_algorithm_iterators.hpp"

namespace rcspp {
template <typename ResourceType>
  requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class SimpleDominanceAlgorithmIterators : public DominanceAlgorithmIterators<ResourceType> {
  public:
    SimpleDominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
                                      const Graph<ResourceType>& graph, bool use_pool = true)
        : DominanceAlgorithmIterators<ResourceType>(resource_factory, graph, use_pool) {}

    ~SimpleDominanceAlgorithmIterators() override = default;

  private:
    LabelIteratorPair<ResourceType> next_label_iterator() override {
      auto label_iterator_pair = unprocessed_labels_.front();

      unprocessed_labels_.pop_front();

      return label_iterator_pair;
    }

    [[nodiscard]] size_t number_of_labels() const override { return unprocessed_labels_.size(); }

    void add_new_unprocessed_label(
      const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
      unprocessed_labels_.push_back(label_iterator_pair);
    }

    std::list<LabelIteratorPair<ResourceType>> unprocessed_labels_;
};
}  // namespace rcspp
