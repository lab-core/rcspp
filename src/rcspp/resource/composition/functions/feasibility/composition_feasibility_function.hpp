// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionFeasibilityFunction
    : public Clonable<CompositionFeasibilityFunction<ResourceTypes...>,
                      FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        CompositionFeasibilityFunction() = default;

        [[nodiscard]] bool is_feasible(const Resource<ResourceTypeComposition<ResourceTypes...>>&
                                           resource_composition) override {
            return feasible_helper(resource_composition,
                                   [](const auto& res_comp) { return res_comp.is_feasible(); });
        }

        [[nodiscard]] bool is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition)
            override {
            return feasible_helper(resource_composition, [](const auto& res_comp) {
                return res_comp.is_back_feasible();
            });
        }

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
