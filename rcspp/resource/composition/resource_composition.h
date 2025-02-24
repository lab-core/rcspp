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

    std::cout << "ResourceComposition(const ResourceComposition& rhs_resource_composition)\n";

    auto clone_res_function = [&](auto& sing_res_vec, const auto& rhs_sing_res_vec) {

      for (const auto& rhs_res : rhs_sing_res_vec) {
        sing_res_vec.emplace_back(std::move(rhs_res->clone()));
      }

      std::cout << "rhs_sing_res_vec: " << rhs_sing_res_vec.size() << std::endl;
      std::cout << "sing_res_vec: " << sing_res_vec.size() << std::endl;

      };

    std::apply([&](auto && ... args_res_comp) {
      std::apply([&](auto && ... args_rhs_res_comp) {

        (clone_res_function(args_res_comp, args_rhs_res_comp), ...);

        }, rhs_resource_composition.resource_components_);
      }, resource_components_);
    
  }

  ResourceComposition(ResourceComposition&& rhs_resource_composition) :
    Resource<ResourceComposition<ResourceTypes...>>() {

    swap(*this, rhs_resource_composition);

    return *this;    
  }

  ~ResourceComposition() {}

  ResourceComposition& operator=(ResourceComposition rhs_resource_composition) {

    swap(*this, rhs_resource_composition);
    
    return *this;
  }

  friend void swap(ResourceComposition& first, ResourceComposition& second) {
    using std::swap;
    swap(first.resource_components_, second.resource_components_);
  }

  template<size_t ResourceTypeIndex, typename ResourceType>
  ResourceType& add_component(std::unique_ptr<ResourceType> resource) {
    return *std::get<ResourceTypeIndex>(resource_components_).emplace_back(std::move(resource));
  }

  template<size_t ResourceTypeIndex, typename ResourceType, typename TypeTuple>
  ResourceType& add_component(const TypeTuple& resource_initializer) {

    auto res_init_index = std::make_index_sequence<std::tuple_size<
      typename std::remove_reference<decltype(resource_initializer)>::type>::value>{};

    auto resource = make_single_resource<ResourceType>(resource_initializer, res_init_index);
    
    return *std::get<ResourceTypeIndex>(resource_components_).emplace_back(std::move(resource));
  }

  std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& get_components() {
    return resource_components_;
  }

  const std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& get_components() const {
    return resource_components_;
  }

  template<size_t ResourceTypeIndex, typename ResourceType>
  std::vector<std::unique_ptr<ResourceType>>& get_components() {
    return std::get<ResourceTypeIndex>(resource_components_);
  }

  template<size_t ResourceTypeIndex, typename ResourceType>
  const std::vector<std::unique_ptr<ResourceType>>& get_components() const {
    return std::get<ResourceTypeIndex>(resource_components_);
  }

  template<size_t ResourceTypeIndex, typename ResourceType>
  ResourceType& get_components(size_t resource_index) {
    return *std::get<ResourceTypeIndex>(resource_components_)[resource_index];
  }

private:
  std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_components_;

  template<typename ResourceType, typename TypeTuple, std::size_t... N>
  std::unique_ptr<ResourceType> make_single_resource(const TypeTuple& resource_initializer, std::index_sequence<N...>) {

    return std::make_unique<ResourceType>(std::get<N>(resource_initializer)...);
  }

};