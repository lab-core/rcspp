#pragma once

#include "resource/resource_function/dominance/dominance_function.h"
#include "resource/composition/resource_composition.h"
#include "general/clonable.h"


template<typename... ResourceTypes>
class CompositionDominanceFunction : public Clonable<CompositionDominanceFunction<ResourceTypes...>,
  DominanceFunction<ResourceComposition<ResourceTypes...>>> {

  bool check_dominance(const ResourceComposition<ResourceTypes...>& lhs_resource, 
    const ResourceComposition<ResourceTypes...>& rhs_resource) override {

    bool is_less_than_or_equal = true;

    const auto& lhs_resource_components = lhs_resource.get_components();
    const auto& rhs_resource_components = rhs_resource.get_components();

    auto dominance_res_function = [&](const auto& lhs_sing_res_vec, const auto& rhs_sing_res_vec) {

      for (int i = 0; i < lhs_sing_res_vec.size(); i++) {

        if (!(*lhs_sing_res_vec[i] <= *rhs_sing_res_vec[i])) {
          is_less_than_or_equal = false;
          break;
        }
      }

      };

    std::apply([&](auto && ... args_lhs) {
      std::apply([&](auto && ... args_rhs) {

        (dominance_res_function(args_lhs, args_rhs), ...);
                
        }, rhs_resource_components);
      }, lhs_resource_components);

    return is_less_than_or_equal;
  }

};