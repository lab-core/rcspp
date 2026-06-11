// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that checks all components of a composed resource.
///
/// All three feasibility checks — forward feasibility, backward feasibility, and
/// merge feasibility — are evaluated component-wise: the composed resource is
/// feasible only if every constituent sub-resource passes the corresponding check.
///
/// @tparam ResourceTypes The individual resource types forming the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionFeasibilityFunction
    : public Clonable<CompositionFeasibilityFunction<ResourceTypes...>,
                      FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        /// @brief Default constructor.
        CompositionFeasibilityFunction() = default;

        /// @brief Returns `true` if every sub-resource is forward-feasible.
        ///
        /// @param resource_composition The composed resource to check.
        /// @return `true` if `is_feasible()` returns `true` for every sub-resource.
        [[nodiscard]] bool is_feasible(const Resource<ResourceTypeComposition<ResourceTypes...>>&
                                           resource_composition) override {
            return feasible_helper(resource_composition,
                                   [](const auto& res_comp) { return res_comp.is_feasible(); });
        }

        /// @brief Returns `true` if every sub-resource is backward-feasible.
        ///
        /// @param resource_composition The composed resource to check.
        /// @return `true` if `is_back_feasible()` returns `true` for every sub-resource.
        [[nodiscard]] bool is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition)
            override {
            return feasible_helper(resource_composition, [](const auto& res_comp) {
                return res_comp.is_back_feasible();
            });
        }

        /// @brief Returns `true` if every component pair can be merged.
        ///
        /// Checks `res.can_be_merged(back_res)` for each paired sub-resource across the
        /// forward and backward compositions.
        ///
        /// @param resource_composition      The forward composed resource.
        /// @param back_resource_composition The backward composed resource to merge with.
        /// @return `true` if every paired sub-resource can be merged.
        [[nodiscard]] bool can_be_merged(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& back_resource_composition)
            override {
            return resource_composition.for_each_component_and(
                back_resource_composition,
                [](const auto& res, const auto& back_res) { return res.can_be_merged(back_res); });
        }

    private:
        template <typename F>
        [[nodiscard]] bool feasible_helper(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition,
            const F& feasible_func) const {
            return resource_composition.for_each_component_and(
                [&](const auto& res) { return feasible_func(res); });
        }
};
}  // namespace rcspp
