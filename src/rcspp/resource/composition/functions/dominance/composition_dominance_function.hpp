// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

// TODO(patrick): Define dominance_res_function as a method.

template <typename... ResourceTypes>
class CompositionDominanceFunction
    : public Clonable<CompositionDominanceFunction<ResourceTypes...>,
                      DominanceFunction<ResourceComposition<ResourceTypes...>>> {
    public:
        CompositionDominanceFunction() = default;

        bool check_dominance(
            const Resource<ResourceComposition<ResourceTypes...>>& lhs_resource,
            const Resource<ResourceComposition<ResourceTypes...>>& rhs_resource) override {
            // bool is_less_than_or_equal = true;

            is_less_than_or_equal_ = true;

            const auto& lhs_resource_components = lhs_resource.get_resource_components();
            const auto& rhs_resource_components = rhs_resource.get_resource_components();

            /*const auto dominance_res_function = [&](const auto& lhs_sing_res_vec, const auto&
              rhs_sing_res_vec) {

              for (int i = 0; i < lhs_sing_res_vec.size(); i++) {
                if (!(*lhs_sing_res_vec[i] <= *rhs_sing_res_vec[i])) {
                  is_less_than_or_equal = false;
                  break;
                }
              }

              return is_less_than_or_equal;

              };*/

            // std::apply([&](auto && ... args_lhs) {
            //   std::apply([&](auto && ... args_rhs) {

            //    // The && operator acts as a break in the fold expression.
            //    (dominance_res_function(args_lhs, args_rhs) && ...);
            //
            //    }, rhs_resource_components);
            //  }, lhs_resource_components);

            std::apply(
                [&](auto&&... args_lhs) {
                    std::apply(
                        [&](auto&&... args_rhs) {
                            // The && operator acts as a break in the fold expression.
                            (check_dominance(args_lhs, args_rhs) && ...);
                        },
                        rhs_resource_components);
                },
                lhs_resource_components);

            return is_less_than_or_equal_;
        }

    private:
        bool is_less_than_or_equal_{true};

        bool check_dominance(const auto& lhs_sing_res_vec, const auto& rhs_sing_res_vec) {
            for (int i = 0; i < lhs_sing_res_vec.size(); i++) {
                if (!(*lhs_sing_res_vec[i] <= *rhs_sing_res_vec[i])) {
                    is_less_than_or_equal_ = false;
                    break;
                }
            }

            return is_less_than_or_equal_;
        }
};
