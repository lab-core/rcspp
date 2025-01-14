#pragma once

#include "resource/resource_function/cost/cost_function.h"
#include "resource/composition/resource_composition.h"
#include "general/clonable.h"


template<typename... ResourceTypes>
class CompositionCostFunction : public Clonable<CompositionCostFunction<ResourceTypes...>,
  CostFunction<ResourceComposition<ResourceTypes...>>> {

public:
  virtual double get_cost(const ResourceComposition<ResourceTypes...>& resource_composition) const override {

    double total_cost = 0;

    const auto& resource_components = resource_composition.get_components();

    auto res_cost_function = [&](const auto& sing_res_vec) {

      for (auto&& res_comp : sing_res_vec) {
        total_cost += res_comp->get_cost();
      }

      };

    std::apply([&](auto && ... args) {

      (res_cost_function(args), ...);

      }, resource_components);

    return total_cost;
  }
};