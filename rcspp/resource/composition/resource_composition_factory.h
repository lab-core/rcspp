#pragma once

#include "resource/resource_factory.h"
#include "resource_composition.h"

#include "resource/composition/expansion/composition_expansion_function.h"
#include "resource/composition/feasibility/composition_feasibility_function.h"
#include "resource/composition/cost/composition_cost_function.h"
#include "resource/composition/dominance/composition_dominance_function.h"


template<typename... ResourceTypes, typename... ResourceFactoryTypes>
class ResourceCompositionFactory<std::tuple<ResourceTypes...>, 
  std::tuple<ResourceFactoryTypes...>> : public ResourceFactory<ResourceComposition<ResourceTypes...>> {

public:

  /*ResourceCompositionFactory()
  {
    std::cout << "ResourceCompositionFactory\n";
  }*/

  ResourceCompositionFactory(std::tuple<std::unique_ptr<ResourceFactoryTypes>...> resource_factories)
    : resource_factories_(std::move(resource_factories))
  {
    std::cout << "ResourceCompositionFactory(std::tuple<std::unique_ptr<ResourceFactoryTypes>...> resource_factories)\n";
  }

  ResourceCompositionFactory(std::tuple<std::unique_ptr<ResourceFactoryTypes>...> resource_factories, 
    std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>> expansion_function,
    std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>> feasibility_function,
    std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
    std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function
    )
    : ResourceFactory<ResourceComposition<ResourceTypes...>>(
      std::move(expansion_function), std::move(feasibility_function),
      std::move(cost_function), std::move(dominance_function)),
    resource_factories_(std::move(resource_factories))
  {
    std::cout << "ResourceCompositionFactory(resource_factories, expansion_function, feasibility_function, cost_function, dominance_function)\n";
  }

  ResourceCompositionFactory(std::unique_ptr<ResourceComposition<ResourceTypes...>> resource_prototype)
    : ResourceFactory<ResourceComposition<ResourceTypes...>>(std::move(resource_prototype)) {

    // TODO: Create factories from prototype.

  }

  /*ResourceCompositionFactory(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components)
     :
    ResourceFactory<ResourceComposition<ResourceTypes...>>(std::make_unique<ResourceComposition<ResourceTypes...>>(
    std::make_unique<CompositionExpansionFunction<ResourceTypes...>>(),
    std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
    std::make_unique<CompositionCostFunction<ResourceTypes...>>(),
    std::make_unique<CompositionDominanceFunction<ResourceTypes...>>(),
    std::move(resource_components)
  )) 
  {

  }*/
    
  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components) {

    std::cout << "ResourceCompositionFactory::make_resource\n";

    auto cloned_resource = this->resource_prototype_->clone();

    //cloned_resource->get_components() = std::move(resource_components);

    cloned_resource->resource_components_ = std::move(resource_components);

    return cloned_resource;
  }

  /*std::unique_ptr<ResourceComposition<ResourceTypes...>> make_default_resource(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components) {

    auto resource = std::make_unique<ResourceComposition<ResourceTypes...>>(
      std::make_unique<CompositionExpansionFunction<ResourceTypes...>>(),
      std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
      std::make_unique<CompositionCostFunction<ResourceTypes...>>(),
      std::make_unique<CompositionDominanceFunction<ResourceTypes...>>(),
      std::move(resource_components)
    );

    return resource;
  }*/

  template<typename... TypeTuples>
  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource(const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {

    std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_components;
        
    auto make_resource_function = [&](const auto& factory, const auto& res_init_vec, auto& res_comp_vec) {

      for (auto&& res_init : res_init_vec) {

        auto res_init_index = std::make_index_sequence<std::tuple_size<typename std::remove_reference<decltype(res_init)>::type>::value>{};

        auto single_resource = make_single_resource(factory, res_init, res_init_index);

        res_comp_vec.emplace_back(std::move(single_resource));
      }

      };

    std::apply([&](auto && ... args_factory) {
      std::apply([&](auto && ... args_res_init_vec) {
        std::apply([&](auto && ... args_res_comp) {

          (make_resource_function(args_factory, args_res_init_vec, args_res_comp), ...);

          }, resource_components);

        }, resource_initializer);
      }, resource_factories_);

    auto resource_composition = make_resource(resource_components);

    return resource_composition;
  }

private:
  std::tuple<std::unique_ptr<ResourceFactoryTypes>...> resource_factories_;

  template<typename FactoryType, typename TypeTuple, std::size_t... N>
  auto make_single_resource(const FactoryType& factory, TypeTuple tuple, std::index_sequence<N...>) {
    return factory->make_resource(std::get<N>(tuple)...);
  }

};
