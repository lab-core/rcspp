// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class CompositionExpansionFunction
    : public Clonable<CompositionExpansionFunction<ResourceTypes...>,
                      ExpansionFunction<ResourceComposition<ResourceTypes...>>> {
  public:
    void expand(const Resource<ResourceComposition<ResourceTypes...>>& resource,
                const Expander<ResourceComposition<ResourceTypes...>>& expander,
                Resource<ResourceComposition<ResourceTypes...>>* expanded_resource) override {
      auto& resource_components = resource.get_resource_components();
      auto& expander_components = expander.get_expander_components();
      auto& expanded_resource_components = expanded_resource->get_resource_components();

      /*const auto expand_res_function = [&](const auto& sing_res_vec, const auto&
        sing_exp_vec, const auto& expanded_sing_res_vec) {

        for (int i = 0; i < sing_res_vec.size(); i++) {
          sing_exp_vec[i]->expand(*sing_res_vec[i], *expanded_sing_res_vec[i]);
        }
        };*/

      /*std::apply([&](auto && ... args_res) {
        std::apply([&](auto && ... args_exp) {
          std::apply([&](auto && ... args_expanded) {

            (expand_res_function(args_res, args_exp, args_expanded), ...);

            }, expanded_resource_components);
          }, expander_components);
        }, resource_components);*/

      std::apply(
        [&](auto&&... args_res) {
          std::apply(
            [&](auto&&... args_exp) {
              std::apply(
                [&](auto&&... args_expanded) {
                  (expand_resource(args_res, args_exp, args_expanded), ...);
                },
                expanded_resource_components);
            },
            expander_components);
        },
        resource_components);
    }

  private:
    void expand_resource(const auto& sing_res_vec, const auto& sing_exp_vec,
                         const auto& expanded_sing_res_vec) const {
      for (int i = 0; i < sing_res_vec.size(); i++) {
        sing_exp_vec[i]->expand(*sing_res_vec[i], expanded_sing_res_vec[i].get());
      }
    }
};
}  // namespace rcspp
