#include "composition_cost_function.h"


double CompositionCostFunction::get_cost(const ResourceComposition& resource_composition) const {

  double total_cost = 0;

  const auto& resource_components = resource_composition.get_components();

  for (auto&& res_comp : resource_components) {
    total_cost += res_comp->get_cost();
  }

  return total_cost;
}
