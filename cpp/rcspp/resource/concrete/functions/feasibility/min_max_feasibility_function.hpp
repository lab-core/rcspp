// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        MinMaxFeasibilityFunction(ValueType min, ValueType max, bool merge_by_increasing_value)
            : default_min_(min),
              default_max_(max),
              min_(min),
              max_(max),
              merge_by_increasing_value_(merge_by_increasing_value) {}

        MinMaxFeasibilityFunction(
            ValueType default_min, ValueType default_max,
            std::map<size_t, std::pair<ValueType, ValueType>> min_max_by_node_id = {})
            : min_max_by_node_id_(
                  min_max_by_node_id.empty()
                      ? nullptr
                      : std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                            std::move(min_max_by_node_id))),
              default_min_(default_min),
              default_max_(default_max),
              min_(default_min),
              max_(default_max) {}

        [[nodiscard]] auto is_feasible(const ResourceType& resource) -> bool override {
            return resource.geq(min_) && resource.leq(max_);
        }

        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            if (merge_by_increasing_value_) {
                return resource.get_value() <= back_resource.get_value();
            }
            return resource.get_value() >= back_resource.get_value();
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            min_max_by_node_id_;
        ValueType default_min_{};
        ValueType default_max_{};
        ValueType min_;
        ValueType max_;

        // true: merge by increasing value, false: decreasing value
        // increasing value means that resource.get_value() <= back_resource.get_value()
        bool merge_by_increasing_value_ = true;

        void preprocess(size_t node_id) override {
            if (min_max_by_node_id_ == nullptr) {
                return;
            }
            auto it = min_max_by_node_id_->find(node_id);
            if (it != min_max_by_node_id_->end()) {
                min_ = it->second.first;
                max_ = it->second.second;
            } else {
                min_ = default_min_;
                max_ = default_max_;
            }
        }
};
}  // namespace rcspp
