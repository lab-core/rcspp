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
            // bool is_feasible = true;
            is_feasible_ = true;

            const auto& resource_components = resource_composition.get_resource_components();

            /*const auto is_res_feas_function = [&](const auto& sing_res_vec) {
              for (auto&& res_comp : sing_res_vec) {
                if (!res_comp->is_feasible()) {
                  is_feasible = false;
                  break;
                }

              };

              return is_feasible;

              };*/

            // std::apply([&](auto && ... args) {

            //  // The && operator acts as a break in the fold expression.
            //  (is_res_feas_function(args) && ...);

            //  }, resource_components);

            std::apply(
                [&](auto&&... args) {
                    // The && operator acts as a break in the fold expression.
                    (check_feasibility(args) && ...);
                },
                resource_components);

            return is_feasible_;
        }

    private:
        bool is_feasible_{true};

        bool check_feasibility(const auto& sing_res_vec) {
            for (auto&& res_comp : sing_res_vec) {
                if (!res_comp->is_feasible()) {
                    is_feasible_ = false;
                    break;
                }
            }

            return is_feasible_;
        }
};
}  // namespace rcspp
