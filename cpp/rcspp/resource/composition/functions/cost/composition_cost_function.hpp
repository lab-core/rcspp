// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CompositionCostFunction
    : public Clonable<CompositionCostFunction<ResourceTypes...>,
                      CostFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        // GCOVR_EXCL_START
        // Default placeholder: cost is handled per-component via ComponentCostFunction.
        // clone_factory() creates this but immediately overwrites it via copy-assignment,
        // so get_cost() is never called in practice.
        [[nodiscard]] double get_cost(const Resource<ResourceTypeComposition<ResourceTypes...>>&
                                          resource_composition) const override {
            double total_cost = 0;
            resource_composition.for_each_component(
                [&](const auto& res_comp) { total_cost += res_comp.get_cost(); });
            return total_cost;
        }
        // GCOVR_EXCL_STOP
};
}  // namespace rcspp
