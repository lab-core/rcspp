// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionFeasibilityFunction
    : public Clonable<CompositionFeasibilityFunction<ResourceTypes...>,
                      FeasibilityFunction<ResourceComposition<ResourceTypes...>>> {
    public:
        CompositionFeasibilityFunction() = default;

        bool is_feasible(
            const Resource<ResourceComposition<ResourceTypes...>>& resource_composition) override {
            const auto& resource_components = resource_composition.get_resource_components();

            bool is_feasible = true;
            std::apply(
                [&](auto&&... args) {
                    // The && operator acts as a break in the fold expression.
                    (check_feasibility(args, &is_feasible) && ...);
                },
                resource_components);

            return is_feasible;
        }

    private:
        bool check_feasibility(const auto& sing_res_vec, bool* is_feasible) {
            for (auto&& res_comp : sing_res_vec) {
                if (!res_comp->is_feasible()) {
                    *is_feasible = false;
                    return false;
                }
            }
            return true;
        }
};
}  // namespace rcspp
