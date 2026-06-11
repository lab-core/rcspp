// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

/// @brief Cost function that returns the scalar value of a numeric resource directly as
///        the label cost.
///
/// This is the simplest possible cost function: the accumulated resource value (e.g.,
/// total travel time or total distance) is used as-is as the objective cost.  The
/// resource value must be implicitly convertible to `double`.
///
/// @tparam ResourceType The resource type whose value supports `get_value()` returning
///         a type implicitly convertible to `double`.
template <typename ResourceType>
class ValueCostFunction
    : public Clonable<ValueCostFunction<ResourceType>, CostFunction<ResourceType>> {
    public:
        /// @brief Returns the resource's scalar value as the label cost.
        ///
        /// @param num_resource The numeric resource whose accumulated value represents the cost.
        /// @return The resource value cast to `double`.
        [[nodiscard]] auto get_cost(const ResourceType& num_resource) const -> double override {
            return num_resource.get_value();
        }
};
}  // namespace rcspp
