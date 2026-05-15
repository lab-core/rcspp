// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

// TODO(patrick): Define dominance_res_function as a method.

namespace rcspp {

template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionDominanceFunction
    : public Clonable<CompositionDominanceFunction<ResourceTypes...>,
                      DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        CompositionDominanceFunction() = default;

        [[nodiscard]] bool check_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_composition,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_composition) override {
            return lhs_composition.for_each_component_and(
                rhs_composition,
                [](const auto& lhs_res, const auto& rhs_res) { return lhs_res <= rhs_res; });
        }
};
}  // namespace rcspp
