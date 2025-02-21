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
    std::cout << "ResourceCompositionFactory()\n";
  }

  ResourceCompositionFactory(
    std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>> expansion_function,
    std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>> feasibility_function,
    std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
    std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function
    )
    : ResourceFactory<ResourceComposition<ResourceTypes...>>(
      std::move(expansion_function), std::move(feasibility_function),
      std::move(cost_function), std::move(dominance_function))
  {
    std::cout << "ResourceCompositionFactory(expansion_function, feasibility_function, cost_function, dominance_function)\n";
    
  }

  ResourceCompositionFactory(std::unique_ptr<ResourceComposition<ResourceTypes...>> resource_prototype)
    : ResourceFactory<ResourceComposition<ResourceTypes...>>(std::move(resource_prototype)) {
    std::cout << "ResourceCompositionFactory(std::unique_ptr<ResourceComposition<ResourceTypes...>> resource_prototype)\n";

  }

  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource() {

    return ResourceFactory<ResourceComposition<ResourceTypes...>>::make_resource();
  }
      
  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource(std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& resource_components) {

    std::cout << "ResourceCompositionFactory::make_resource\n";

    auto cloned_resource = this->resource_prototype_->clone();

    cloned_resource->resource_components_ = std::move(resource_components);

    return cloned_resource;
  }


  template<typename... TypeTuples>
  std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource(const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {

    auto new_resource_composition = make_resource();

    auto make_resource_function = [&](const auto& res_init_vec, auto& res_comp_vec) {

      for (int i = 0; i < res_init_vec.size(); i++) {

        const auto& res_init = res_init_vec[i];
        auto& res_comp = res_comp_vec[i];

        auto res_init_index = std::make_index_sequence<std::tuple_size<typename std::remove_reference<decltype(res_init)>::type>::value>{};

        set_value_single_resource(res_comp, res_init, res_init_index);
      }

      };

    std::apply([&](auto && ... args_res_init_vec) {
      std::apply([&](auto && ... args_res_comp) {

        (make_resource_function(args_res_init_vec, args_res_comp), ...);

        }, new_resource_composition->resource_components_);

      }, resource_initializer);


    return new_resource_composition;
  }

private:

  template<typename ResourceType, typename TypeTuple, std::size_t... N>
  auto set_value_single_resource(const ResourceType& resource, const TypeTuple& resource_initializer, std::index_sequence<N...>) {
    resource->set_value(std::get<N>(resource_initializer)...);
  }
    
};
