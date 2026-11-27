// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>
#include <typeinfo>

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

        /// @brief Whether this cost is additive: every component's cost must be.
        ///
        /// @param resource_composition Any node's resource.
        /// @return See @c Resource::is_cost_additive.
        [[nodiscard]] auto is_additive(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition) const
            -> bool override {
            bool additive = true;
            resource_composition.for_each_component(
                [&](const auto& res_comp) { additive = additive && res_comp.is_cost_additive(); });
            return additive;
        }

        /// @brief The cost order of the sum, when at most one component's cost may be nonzero:
        ///        that component's @c Resource::component_cost_order_tolerance (or
        ///        @c back_component_cost_order_tolerance), or 0 when every cost is zero.
        ///
        /// Empty when a component tells nothing, and when two or more have a cost that is not
        /// @c CostKind::Zero: each dominance then holds within its tolerance, but the rounding of
        /// the sum, which grows with the components' magnitudes, may break the promise by more
        /// than the one ulp @c CostOrderedLabelList allows. A dominance function that knows a bound
        /// for such a model declares it (@c DominanceFunction::cost_order_tolerance). Adding a
        /// zero cost is exact, so one nonzero cost among zeros keeps its tolerance.
        ///
        /// @param resource_composition Any node's resource.
        /// @param backward             Whether the components backward-dominate.
        /// @return The tolerance, or @c std::nullopt; none for a class derived from this one,
        ///         which may compute its cost otherwise.
        [[nodiscard]] auto component_wise_cost_order_tolerance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition,
            bool backward) const -> std::optional<double> override {
            if (typeid(*this) != typeid(CompositionCostFunction)) {
                return std::nullopt;
            }
            std::optional<double> tolerance = 0.0;
            size_t nonzero_costs = 0;
            resource_composition.for_each_component([&](const auto& res_comp) {
                if (!tolerance.has_value()) {
                    return;
                }
                const auto component = backward ? res_comp.back_component_cost_order_tolerance()
                                                : res_comp.component_cost_order_tolerance();
                if (!component.has_value()) {
                    tolerance.reset();
                } else if (res_comp.cost_kind() != CostKind::Zero) {
                    ++nonzero_costs;
                    tolerance = nonzero_costs > 1 ? std::nullopt : component;
                }
            });
            return tolerance;
        }
};
}  // namespace rcspp
