#include "composition_feasibility_function.h"


bool CompositionFeasibilityFunction::is_feasible(const ResourceComposition& resource_composition) {

  bool is_feasible = true;

  const auto& resource_components = resource_composition.get_components();

  for (auto&& res_comp : resource_components) {
    if (!res_comp->is_feasible()) {
      is_feasible = false;
      break;
    }
  }

  return is_feasible;
}
