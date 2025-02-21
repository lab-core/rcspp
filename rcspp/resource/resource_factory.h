#pragma once

#include "resource.h"

#include <iostream>


template<typename ResourceType>
class ResourceFactory {

public:

  ResourceFactory() {
  
    std::cout << "ResourceFactory\n";

    make_prototype();
  }

  ResourceFactory(std::unique_ptr<ExpansionFunction<ResourceType>> expansion_function,
    std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
    std::unique_ptr<CostFunction<ResourceType>> cost_function,
    std::unique_ptr<DominanceFunction<ResourceType>> dominance_function) {

    std::cout << "ResourceFactory(expansion_function, feasibility_function, cost_function, dominance_function)\n";

    make_prototype(std::move(expansion_function), std::move(feasibility_function), 
      std::move(cost_function), std::move(dominance_function));
  }

  ResourceFactory(std::unique_ptr<ResourceType> resource_prototype) : 
    resource_prototype_(std::move(resource_prototype)), nb_resources_created_(0) {}

  std::unique_ptr<ResourceType> make_resource() {

    nb_resources_created_++;

    return resource_prototype_->clone();
  }

  

protected:
  
  // Create a default prototype.
  void make_prototype() {

    std::cout << "make_prototype()\n";
    std::cout << typeid(ResourceType).name() << std::endl;

    resource_prototype_ = std::make_unique<ResourceType>();
  }

  // Create a prototype with specific functions.
  void make_prototype(std::unique_ptr<ExpansionFunction<ResourceType>> expansion_function,
    std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
    std::unique_ptr<CostFunction<ResourceType>> cost_function,
    std::unique_ptr<DominanceFunction<ResourceType>> dominance_function) {

    std::cout << "make_prototype(expansion_function, feasibility_function, cost_function, dominance_function)\n";

    resource_prototype_ = std::make_unique<ResourceType>(std::move(expansion_function), 
      std::move(feasibility_function), std::move(cost_function), std::move(dominance_function));
  }

  std::unique_ptr<ResourceType> resource_prototype_;

  size_t nb_resources_created_;
  ;
};