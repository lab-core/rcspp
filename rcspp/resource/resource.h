#pragma once

#include "resource_function/expansion/expansion_function.h"
#include "resource_function/feasibility/feasibility_function.h"
#include "resource_function/cost/cost_function.h"
#include "resource/resource_function/dominance/dominance_function.h"

#include "resource_function/expansion/trivial_expansion_function.h"
#include "resource_function/feasibility/trivial_feasibility_function.h"
#include "resource_function/cost/trivial_cost_function.h"
#include "resource_function/dominance/trivial_dominance_function.h"

#include <memory>
#include <iostream>


template<typename DerivedResourceType>
class Resource {

public:

  Resource() : expansion_function_(std::make_unique<TrivialExpansionFunction<DerivedResourceType>>()),
    feasibility_function_(std::make_unique<TrivialFeasibilityFunction<DerivedResourceType>>()),
    cost_function_(std::make_unique<TrivialCostFunction<DerivedResourceType>>()),
    dominance_function_(std::make_unique<TrivialDominanceFunction<DerivedResourceType>>()) {

  }

  Resource(std::unique_ptr<ExpansionFunction<DerivedResourceType>> expansion_function,
    std::unique_ptr<FeasibilityFunction<DerivedResourceType>> feasibility_function,
    std::unique_ptr<CostFunction<DerivedResourceType>> cost_function,
    std::unique_ptr<DominanceFunction<DerivedResourceType>> dominance_function) :
    expansion_function_(std::move(expansion_function)), feasibility_function_(std::move(feasibility_function)),
    cost_function_(std::move(cost_function)), dominance_function_(std::move(dominance_function)) {

  }

  Resource(Resource const& rhs_resource) : 
    expansion_function_(rhs_resource.expansion_function_->clone()), 
    feasibility_function_(rhs_resource.feasibility_function_->clone()), 
    cost_function_(rhs_resource.cost_function_->clone()), 
    dominance_function_(rhs_resource.dominance_function_->clone()) {

    std::cout << "Resource::Resource(Resource const& rhs_resource)\n";

  }

  /*virtual ~Resource();

  Resource& operator=(const Resource& rhs_resource);*/

  //! Check dominance
  bool operator<=(const DerivedResourceType& rhs_resource) const {

    return dominance_function_->check_dominance(static_cast<DerivedResourceType const&>(*this), rhs_resource);
  }

  //! Resource expansion
  void expand(const DerivedResourceType& rhs_resource, DerivedResourceType& expanded_resource) const {

    expansion_function_->expand(static_cast<DerivedResourceType const&>(*this), rhs_resource, expanded_resource);
  }

  //! Return resource cost
  double get_cost() const {

    return cost_function_->get_cost(static_cast<DerivedResourceType const&>(*this));
  }

  //! Return true if the resource is feasible
  bool is_feasible() const {

    return feasibility_function_->is_feasible(static_cast<DerivedResourceType const&>(*this));
  }

  virtual std::unique_ptr<DerivedResourceType> clone() const {

    return std::make_unique<DerivedResourceType>(static_cast<DerivedResourceType const&>(*this));
  }

protected:
  std::unique_ptr<ExpansionFunction<DerivedResourceType>> expansion_function_;
  std::unique_ptr<FeasibilityFunction<DerivedResourceType>> feasibility_function_;
  std::unique_ptr<CostFunction<DerivedResourceType>> cost_function_;
  std::unique_ptr<DominanceFunction<DerivedResourceType>> dominance_function_;

};