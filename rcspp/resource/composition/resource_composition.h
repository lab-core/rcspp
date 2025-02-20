#pragma once

#include "resource/resource.h"

#include <vector>
#include <memory>


template<typename... Types>
class ResourceCompositionFactory;

template<typename... ResourceTypes>
class CompositionExpansionFunction;

template<typename... ResourceTypes>
class CompositionFeasibilityFunction;

template<typename... ResourceTypes>
class CompositionCostFunction;

template<typename... ResourceTypes>
class CompositionDominanceFunction;


template<typename... ResourceTypes>
class ResourceComposition : public Resource<ResourceComposition<ResourceTypes...>> {
  template<typename... Types>
  friend class ResourceCompositionFactory;

public:

  ResourceComposition() : Resource<ResourceComposition>(
    std::make_unique<CompositionExpansionFunction<ResourceTypes...>>(),
    std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
    std::make_unique<CompositionCostFunction<ResourceTypes...>>(),
    std::make_unique<CompositionDominanceFunction<ResourceTypes...>>()) {

  }

  /*ResourceComposition(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components) : 
    Resource<ResourceComposition>(
      std::make_unique<CompositionExpansionFunction>(),
      std::make_unique<CompositionFeasibilityFunction>(),
      std::make_unique<CompositionCostFunction>(),
      std::make_unique<CompositionDominanceFunction>()), 
    resource_components_(std::move(resource_components)) {

  }  */ 

  ResourceComposition(std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>> expansion_function,
    std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>> feasibility_function,
    std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
    std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function) :
    Resource<ResourceComposition<ResourceTypes...>>(
      std::move(expansion_function),
      std::move(feasibility_function),
      std::move(cost_function),
      std::move(dominance_function)),
    resource_components_(std::make_unique<std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>>()) {

  }

  ResourceComposition(std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>> expansion_function,
    std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>> feasibility_function,
    std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
    std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function,
    std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_components) :
    Resource<ResourceComposition<ResourceTypes...>>(
      std::move(expansion_function),
      std::move(feasibility_function),
      std::move(cost_function),
      std::move(dominance_function)),
    resource_components_(std::move(resource_components)) {

  }

  ResourceComposition(const ResourceComposition& rhs_resource_composition) : 
    Resource<ResourceComposition<ResourceTypes...>>(rhs_resource_composition)
  {
    // resource_components_ is neither copied nor moved.
  }

  ~ResourceComposition() {}

  ResourceComposition& operator=(const ResourceComposition& rhs_resource_composition) {
    Resource<ResourceComposition<ResourceTypes...>>::operator=(rhs_resource_composition);
  }

  std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& get_components() {
    return resource_components_;
  }

  const std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& get_components() const {
    return resource_components_;
  }

private:
  std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_components_;

};