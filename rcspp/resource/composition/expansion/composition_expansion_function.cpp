#include "composition_expansion_function.h"


void CompositionExpansionFunction::expand(const ResourceComposition& lhs_resource,
  const ResourceComposition& rhs_resource, ResourceComposition& expanded_resource) {

  const auto& lhs_resource_components = lhs_resource.get_components();
  const auto& rhs_resource_components = rhs_resource.get_components();
  auto& expanded_resource_components = expanded_resource.get_components();

  std::vector<std::unique_ptr<Resource>> resource_components;

  for (int i = 0; i < lhs_resource_components.size(); i++) {

    auto&& lhs_resource_compoment = lhs_resource_components[i];
    auto&& rhs_resource_component = rhs_resource_components[i];
    auto&& expanded_resource_component = expanded_resource_components[i];

    lhs_resource_compoment->expand(*rhs_resource_component, *expanded_resource_component);
  }
}
