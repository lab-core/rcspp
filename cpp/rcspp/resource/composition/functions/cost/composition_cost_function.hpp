// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

/// @brief Cost function that sums the costs of all components in a composed resource.
///
/// Iterates over every sub-resource in every type slot of the composition and accumulates
/// their individual costs via `get_cost()`.
///
/// @tparam ResourceTypes The individual resource types forming the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionCostFunction
    : public Clonable<CompositionCostFunction<ResourceTypes...>,
                      CostFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        /// @brief Returns the total cost as the sum of all component costs.
        ///
        /// @param resource_composition The composed resource to evaluate.
        /// @return Sum of `get_cost()` over every sub-resource in the composition.
        [[nodiscard]] double get_cost(const Resource<ResourceTypeComposition<ResourceTypes...>>&
                                          resource_composition) const override {
            double total_cost = 0;
            resource_composition.for_each_component(
                [&](const auto& res_comp) { total_cost += res_comp.get_cost(); });
            return total_cost;
        }
};
}  // namespace rcspp
