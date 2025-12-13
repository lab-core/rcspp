// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionCostFunction
    : public Clonable<CompositionCostFunction<ResourceTypes...>,
                      CostFunction<ResourceBaseComposition<ResourceTypes...>>> {
    public:
        double get_cost(const Resource<ResourceBaseComposition<ResourceTypes...>>&
                            resource_composition) const override {
            double total_cost = 0;
            resource_composition.for_each_component(
                [&](const auto& res_comp) { total_cost += res_comp.get_cost(); });
            return total_cost;
        }
};
}  // namespace rcspp
