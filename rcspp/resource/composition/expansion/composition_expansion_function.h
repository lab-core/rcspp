#pragma once

#include "resource/resource_function/expansion/expansion_function.h"
#include "resource/composition/resource_composition.h"
#include "general/clonable.h"

#include <tuple>
#include <iostream>


template<typename... ResourceTypes>
class CompositionExpansionFunction : public Clonable<CompositionExpansionFunction<ResourceTypes...>,
  ExpansionFunction<ResourceComposition<ResourceTypes...>>> {

  void expand(const ResourceComposition<ResourceTypes...>& lhs_resource, const ResourceComposition<ResourceTypes...>& rhs_resource,
    ResourceComposition<ResourceTypes...>& expanded_resource) override {

    std::cout << "CompositionExpansionFunction::expand\n";

    auto& lhs_resource_components = lhs_resource.get_components();
    auto& rhs_resource_components = rhs_resource.get_components();
    auto& expanded_resource_components = expanded_resource.get_components();

    auto expand_res_function = [&](const auto& lhs_sing_res_vec, const auto& rhs_sing_res_vec, const auto& expanded_sing_res_vec) {

      std::cout << "lhs_sing_res_vec: " << lhs_sing_res_vec.size() << std::endl;
      std::cout << "rhs_sing_res_vec: " << rhs_sing_res_vec.size() << std::endl;
      std::cout << "expanded_sing_res_vec: " << expanded_sing_res_vec.size() << std::endl;

      for (int i = 0; i < lhs_sing_res_vec.size(); i++) {
        lhs_sing_res_vec[i]->expand(*rhs_sing_res_vec[i], *expanded_sing_res_vec[i]);
      }

      std::cout << "lhs_sing_res_vec AFTER: " << lhs_sing_res_vec.size() << std::endl;
      std::cout << "rhs_sing_res_vec AFTER: " << rhs_sing_res_vec.size() << std::endl;
      std::cout << "expanded_sing_res_vec AFTER: " << expanded_sing_res_vec.size() << std::endl;

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