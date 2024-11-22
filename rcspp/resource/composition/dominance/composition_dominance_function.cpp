#include "composition_dominance_function.h"


bool CompositionDominanceFunction::check_dominance(const ResourceComposition& lhs_resource, const ResourceComposition& rhs_resource) {

  bool is_less_than_or_equal = true;

  const auto& lhs_resource_components = lhs_resource.get_components();
  const auto& rhs_resource_components = rhs_resource.get_components();

  for (int i = 0; i < lhs_resource_components.size(); i++) {

    auto&& lhs_res_comp = lhs_resource_components[i];
    auto&& rhs_res_comp = rhs_resource_components[i];

    if (!(*lhs_res_comp <= *rhs_res_comp)) {
      is_less_than_or_equal = false;
      break;
    }
  }

  return is_less_than_or_equal;
}