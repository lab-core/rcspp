// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

// TODO(patrick): Define dominance_res_function as a method.

namespace rcspp {

/// @brief Dominance function that checks dominance component-wise across a composed resource.
///
/// A composed resource @p lhs is considered to dominate @p rhs if and only if every
/// constituent sub-resource in @p lhs dominates (via `operator<=`) the corresponding
/// sub-resource in @p rhs.
///
/// @tparam ResourceTypes The individual resource types forming the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionDominanceFunction
    : public Clonable<CompositionDominanceFunction<ResourceTypes...>,
                      DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        /// @brief Default constructor.
        CompositionDominanceFunction() = default;

        /// @brief Returns `true` if @p lhs_composition dominates @p rhs_composition.
        ///
        /// Dominance holds when every paired sub-resource satisfies `lhs_res <= rhs_res`.
        ///
        /// @param lhs_composition The candidate dominating resource.
        /// @param rhs_composition The resource being compared against.
        /// @return `true` if @p lhs_composition dominates @p rhs_composition.
        [[nodiscard]] bool check_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_composition,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_composition) override {
            return lhs_composition.for_each_component_and(
                rhs_composition,
                [](const auto& lhs_res, const auto& rhs_res) { return lhs_res <= rhs_res; });
        }

        /// @brief Returns `true` if @p lhs_composition backward-dominates @p rhs_composition.
        ///
        /// Component-wise, exactly like @c check_dominance() -- each component applies its *own*
        /// direction, which is what lets cost stay unreversed while a time window reverses within
        /// the same composition.
        ///
        /// @param lhs_composition The candidate dominating resource.
        /// @param rhs_composition The resource being compared against.
        /// @return `true` if every component of @p lhs_composition backward-dominates the
        ///         corresponding component of @p rhs_composition.
        [[nodiscard]] bool check_back_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_composition,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_composition) override {
            return lhs_composition.for_each_component_and(
                rhs_composition,
                [](const auto& lhs_res, const auto& rhs_res) {
                    return lhs_res.back_dominates(rhs_res);
                });
        }
};
}  // namespace rcspp
