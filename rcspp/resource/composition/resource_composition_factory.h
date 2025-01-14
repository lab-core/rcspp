#pragma once

#include "resource/resource_factory.h"
#include "resource_composition.h"

#include "resource/composition/expansion/composition_expansion_function.h"
#include "resource/composition/feasibility/composition_feasibility_function.h"
#include "resource/composition/cost/composition_cost_function.h"
#include "resource/composition/dominance/composition_dominance_function.h"


template<typename... ResourceTypes>
class ResourceCompositionFactory : public ResourceFactory<ResourceComposition<ResourceTypes...>> {

public:

  ResourceCompositionFactory()
  {
    std::cout << "ResourceCompositionFactory\n";
  }

  ResourceCompositionFactory(std::tuple<std::vector<std::unique_ptr<ResourceTypes...>>> resource_components)
     :
    ResourceFactory<ResourceComposition<ResourceTypes...>>(std::make_unique<ResourceComposition<ResourceTypes...>>(
    std::make_unique<CompositionExpansionFunction<ResourceTypes...>>(),
    std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
    std::make_unique<CompositionCostFunction<ResourceTypes...>>(),
    std::make_unique<CompositionDominanceFunction<ResourceTypes...>>(),
    std::move(resource_components)
  )) 
  {
    
  }

  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components) {

    auto cloned_resource = this->resource_prototype_->clone();

    cloned_resource->resource_components_ = std::move(resource_components);

    return cloned_resource;
  }

  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_default_resource(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components) {

    auto resource = std::make_unique<ResourceComposition<ResourceTypes...>>(
      std::make_unique<CompositionExpansionFunction<ResourceTypes...>>(),
      std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
      std::make_unique<CompositionCostFunction<ResourceTypes...>>(),
      std::make_unique<CompositionDominanceFunction<ResourceTypes...>>(),
      std::move(resource_components)
    );

    return resource;
  }

};