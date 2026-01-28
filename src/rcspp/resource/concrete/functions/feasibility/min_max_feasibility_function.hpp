// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType =
              std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>>
class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        MinMaxFeasibilityFunction(
            ValueType default_min, ValueType default_max,
            const std::map<size_t, std::pair<ValueType, ValueType>>* min_max_by_node_id = nullptr)
            : min_max_by_node_id_(min_max_by_node_id), min_(default_min), max_(default_max) {}
        MinMaxFeasibilityFunction(
            const std::map<size_t, std::pair<ValueType, ValueType>>* min_max_by_node_id)
            : min_max_by_node_id_(min_max_by_node_id) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return resource.geq(min_) && resource.leq(max_);
        }

    private:
        const std::map<size_t, std::pair<ValueType, ValueType>>* const min_max_by_node_id_;

        ValueType min_;
        ValueType max_;

        void preprocess(size_t node_id) override {
            if (min_max_by_node_id_ == nullptr) {
                return;
            }
            auto it = min_max_by_node_id_->find(node_id);
            // if not found, keep previous min_/max_ values
            if (it != min_max_by_node_id_->end()) {
                min_ = it->second.first;
                max_ = it->second.second;
            }
        }
};
}  // namespace rcspp
