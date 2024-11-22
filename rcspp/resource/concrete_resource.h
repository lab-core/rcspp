#pragma once

#include "resource.h"

#include <memory>

#include <iostream>


template<class DerivedResource>
class ConcreteResource : public Resource {
public:

  ConcreteResource(std::unique_ptr<ExpansionFunction> expansion_function,
    std::unique_ptr<FeasibilityFunction> feasibility_function,
    std::unique_ptr<CostFunction> cost_function,
    std::unique_ptr<DominanceFunction> dominance_function) : 
    Resource(std::move(expansion_function), std::move(feasibility_function), 
      std::move(cost_function), std::move(dominance_function)) {

  }

  virtual std::unique_ptr<Resource> clone() const override {

    std::cout << "ConcreteResource::clone()\n";

    return std::make_unique<DerivedResource>(static_cast<DerivedResource const&>(*this));
  }

  virtual std::unique_ptr<DerivedResource> concrete_clone() const {

    std::cout << "ConcreteResource::concrete_clone()\n";

    return std::make_unique<DerivedResource>(static_cast<DerivedResource const&>(*this));
  }
};