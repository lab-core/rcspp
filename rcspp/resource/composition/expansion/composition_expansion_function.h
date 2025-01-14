#pragma once

#include "resource/resource_function/expansion/expansion_function.h"
#include "resource/composition/resource_composition.h"
#include "general/clonable.h"

#include <tuple>




template<typename... ResourceTypes>
class CompositionExpansionFunction : public Clonable<CompositionExpansionFunction<ResourceTypes...>,
  ExpansionFunction<ResourceComposition<ResourceTypes...>>> {

  void expand(const ResourceComposition<ResourceTypes...>& lhs_resource, const ResourceComposition<ResourceTypes...>& rhs_resource,
    ResourceComposition<ResourceTypes...>& expanded_resource) override {

    auto& lhs_resource_components = lhs_resource.get_components();
    auto& rhs_resource_components = rhs_resource.get_components();
    auto& expanded_resource_components = expanded_resource.get_components();

    auto expand_res_function = [&](const auto& lhs_sing_res_vec, const auto& rhs_sing_res_vec, const auto& expanded_sing_res_vec) {

      for (int i = 0; i < lhs_sing_res_vec.size(); i++) {
        lhs_sing_res_vec[i]->expand(*rhs_sing_res_vec[i], *expanded_sing_res_vec[i]);
      }

      };

    std::apply([&](auto && ... args_lhs) {
      std::apply([&](auto && ... args_rhs) {
        std::apply([&](auto && ... args_expanded) {

          (expand_res_function(args_lhs, args_rhs, args_expanded), ...);

          }, expanded_resource_components);
        }, rhs_resource_components);
      }, lhs_resource_components);

  }
};