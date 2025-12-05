// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

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
        MinMaxFeasibilityFunction(ValueType min, ValueType max,
                                  bool merge_by_increasing_value = true)
            : min_(min), max_(max), merge_by_increasing_value_(merge_by_increasing_value) {}

        [[nodiscard]] auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return resource.geq(min_) && resource.leq(max_);
        }

        [[nodiscard]] auto can_be_merged(const Resource<ResourceType>& resource,
                                         const Resource<ResourceType>& back_resource)
            -> bool override {
            if (merge_by_increasing_value_) {
                return resource.get_value() <= back_resource.get_value();
            }
            return resource.get_value() >= back_resource.get_value();
        }

    private:
        ValueType min_;
        ValueType max_;

        // true: merge by increasing value, false: decreasing value
        // increasing value means that resource.get_value() <= back_resource.get_value()
        bool merge_by_increasing_value_;
};
}  // namespace rcspp
