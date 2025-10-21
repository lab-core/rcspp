// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionCostFunction
    : public Clonable<CompositionCostFunction<ResourceTypes...>,
                      CostFunction<ResourceComposition<ResourceTypes...>>> {
    public:
        double get_cost(const Resource<ResourceComposition<ResourceTypes...>>& resource_composition)
            const override {
            double total_cost = 0;

            const auto& resource_components = resource_composition.get_resource_components();

            const auto res_cost_function = [&](const auto& sing_res_vec) {
                for (auto&& res_comp : sing_res_vec) {
                    total_cost += res_comp->get_cost();
                }
            };

            std::apply([&](auto&&... args) { (res_cost_function(args), ...); },
                       resource_components);

            return total_cost;
        }
};
}  // namespace rcspp