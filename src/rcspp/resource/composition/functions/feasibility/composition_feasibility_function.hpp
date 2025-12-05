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

        [[nodiscard]] bool is_feasible(
            const Resource<ResourceComposition<ResourceTypes...>>& resource_composition) override {
            return feasible_helper(resource_composition,
                                   [](const auto& res_comp) { return res_comp.is_feasible(); });
        }

        [[nodiscard]] bool is_back_feasible(
            const Resource<ResourceComposition<ResourceTypes...>>& resource_composition) override {
            return feasible_helper(resource_composition, [](const auto& res_comp) {
                return res_comp.is_back_feasible();
            });
        }

        [[nodiscard]] bool can_be_merged(
            const Resource<ResourceComposition<ResourceTypes...>>& resource_composition,
            const Resource<ResourceComposition<ResourceTypes...>>& back_resource_composition)
            override {
            return std::apply(
                [&](auto&&... args_res) {
                    return std::apply(
                        [&](auto&&... args_back_res) {
                            // The && operator acts as a break in the fold expression.
                            return (check_merge(args_res, args_back_res) && ...);
                        },
                        back_resource_composition.get_resource_components());
                },
                resource_composition.get_resource_components());
        }

    private:
        template <typename F>
        [[nodiscard]] bool feasible_helper(
            const Resource<ResourceComposition<ResourceTypes...>>& resource_composition,
            const F& feasible_func) const {
            const auto& resource_components = resource_composition.get_resource_components();

            return std::apply(
                [&](auto&&... args_res) {
                    // The && operator acts as a break in the fold expression.
                    return (check_feasibility(args_res, feasible_func) && ...);
                },
                resource_components);
        }

        template <typename F>
        [[nodiscard]] bool check_feasibility(const auto& sing_res_vec,
                                             const F& feasible_func) const {
            return std::ranges::all_of(sing_res_vec, [&](const auto& res_comp) {
                return feasible_func(*res_comp);
            });
        }

        [[nodiscard]] bool check_merge(const auto& sing_res_vec,
                                       const auto& sing_back_res_vec) const {
            for (int i = 0; i < sing_res_vec.size(); i++) {
                if (!sing_res_vec[i]->can_be_merged(*sing_back_res_vec[i])) {
                    return false;
                }
            }
            return true;
        }
};
}  // namespace rcspp
