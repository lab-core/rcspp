//#include "resource/resource.h"
//#include "resource_composition.h"
//
//#include "resource/composition/cost/composition_cost_function.h"
//#include "resource/composition/expansion/composition_expansion_function.h"
//#include "resource/composition/feasibility/composition_feasibility_function.h"
//#include "resource/composition/dominance/composition_dominance_function.h"
//
//#include <memory>
//
//
//ResourceComposition::ResourceComposition(std::vector<std::unique_ptr<Resource>>& resource_components) : 
//  ConcreteResource<ResourceComposition>(
//    std::make_unique<CompositionExpansionFunction>(),
//    std::make_unique<CompositionFeasibilityFunction>(),
//    std::make_unique<CompositionCostFunction>(),
//    std::make_unique<CompositionDominanceFunction>()) {
//
//  for (auto& resource : resource_components) {
//    resource_components_.push_back(std::move(resource));
//  }
//}
//
//ResourceComposition::ResourceComposition(std::unique_ptr<ExpansionFunction> expansion_function,
//  std::unique_ptr<FeasibilityFunction> feasibility_function,
//  std::unique_ptr<CostFunction> cost_function,
//  std::unique_ptr<DominanceFunction> dominance_function,
//  std::vector<std::unique_ptr<Resource>>& resource_components) : 
//  ConcreteResource<ResourceComposition>(
//    std::move(expansion_function),
//    std::move(feasibility_function),
//    std::move(cost_function),
//    std::move(dominance_function)) {
//
//  for (auto& resource : resource_components) {
//    resource_components_.push_back(std::move(resource));
//  }
//}
//
//ResourceComposition::ResourceComposition(const ResourceComposition& rhs_resource_composition) : 
//  ConcreteResource<ResourceComposition>(rhs_resource_composition) {
//
//  for (auto& resource : rhs_resource_composition.resource_components_) {
//    resource_components_.push_back(resource->clone());
//  }
//}
//
//ResourceComposition::~ResourceComposition() {
//
//}
//
//ResourceComposition& ResourceComposition::operator=(ResourceComposition const& rhs_resource_composition) {
//
//  ConcreteResource<ResourceComposition>::operator=(rhs_resource_composition);
//
//  for (auto& resource : rhs_resource_composition.resource_components_) {
//    resource_components_.push_back(resource->clone());
//  }
//
//  return *this;
//}
//
//std::vector<std::unique_ptr<Resource>>& ResourceComposition::get_components() {
//  return resource_components_;
//}
//
//const std::vector<std::unique_ptr<Resource>>& ResourceComposition::get_components() const {
//  return resource_components_;
//}
//
////std::unique_ptr<Resource> ResourceComposition::clone() const {
////
////  std::vector<std::unique_ptr<Resource>> new_resource_components;
////  for (auto& component : resource_components_) {
////    new_resource_components.push_back(component->clone());
////  }
////
////  auto new_resource = std::make_unique<ResourceComposition>(
////    expansion_function_->clone(), 
////    feasibility_function_->clone(),
////    cost_function_->clone(), 
////    dominance_function_->clone(), 
////    new_resource_components);
////
////  return new_resource;
////}