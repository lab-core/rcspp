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
                      FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>> {
    public:
        CompositionFeasibilityFunction() = default;

        [[nodiscard]] bool is_feasible(const Resource<ResourceBaseComposition<ResourceTypes...>>&
                                           resource_composition) override {
            return feasible_helper(resource_composition,
                                   [](const auto& res_comp) { return res_comp.is_feasible(); });
        }

        [[nodiscard]] bool is_back_feasible(
            const Resource<ResourceBaseComposition<ResourceTypes...>>& resource_composition)
            override {
            return feasible_helper(resource_composition, [](const auto& res_comp) {
                return res_comp.is_back_feasible();
            });
        }

        [[nodiscard]] bool can_be_merged(
            const Resource<ResourceBaseComposition<ResourceTypes...>>& resource,
            const Resource<ResourceBaseComposition<ResourceTypes...>>& back_resource) override {
            const auto& resource_composition =
                static_cast<const ResourceComposition<ResourceTypes...>&>(resource);
            const auto& back_resource_composition =
                static_cast<const ResourceComposition<ResourceTypes...>&>(back_resource);
            return resource_composition.apply_and(
                back_resource_composition,
                [&](const auto& res_vec, const auto& back_res_vec) {
                    return check_merge(res_vec, back_res_vec);
                });
        }

    private:
        template <typename F>
        [[nodiscard]] bool feasible_helper(
            const Resource<ResourceBaseComposition<ResourceTypes...>>& resource,
            const F& feasible_func) const {
            const auto& resource_composition =
                static_cast<const ResourceComposition<ResourceTypes...>&>(resource);
            return resource_composition.apply_and([&](const auto& res_vec) {
                return std::ranges::all_of(res_vec, [&](const auto& res_comp) {
                    return feasible_func(*res_comp);
                });
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
