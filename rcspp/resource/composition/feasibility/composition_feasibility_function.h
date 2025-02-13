#pragma once

#include "resource/resource_function/feasibility/feasibility_function.h"
#include "resource/composition/resource_composition.h"
#include "general/clonable.h"


template<typename... ResourceTypes>
class CompositionFeasibilityFunction : public Clonable<CompositionFeasibilityFunction<ResourceTypes...>,
  FeasibilityFunction<ResourceComposition<ResourceTypes...>>> {

public:
  bool is_feasible(const ResourceComposition<ResourceTypes...>& resource_composition) override {

    bool is_feasible = true;

    const auto& resource_components = resource_composition.get_components();

    auto is_res_feas_function = [&](const auto& sing_res_vec) {
      std::cout << "sing_res_vec.size()=" << sing_res_vec.size() << std::endl;
      for (auto&& res_comp : sing_res_vec) {
        std::cout << res_comp->get_cost() << std::endl;
        if (!res_comp->is_feasible()) {
          is_feasible = false;
          break;
        }

      };

      return is_feasible;

      };

    std::apply([&](auto && ... args) {

      // The && operator acts as a break in the fold expression.
      (is_res_feas_function(args) && ...);

      }, resource_components);

    return is_feasible;
  }
};