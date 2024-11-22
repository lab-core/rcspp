#include "resource_composition_factory.h"


std::unique_ptr<ResourceComposition> ResourceCompositionFactory::make_resource(
  std::vector<std::unique_ptr<Resource>>& resource_components) {

  auto cloned_resource = concrete_resource_prototype_->concrete_clone();

  cloned_resource->resource_components_ = std::move(resource_components);

  return cloned_resource;
}