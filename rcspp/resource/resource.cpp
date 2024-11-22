#include "resource.h"

#include "resource_function/expansion/expansion_function.h"
#include "resource_function/feasibility/feasibility_function.h"
#include "resource_function/cost/cost_function.h"
#include "resource/resource_function/dominance/dominance_function.h"

#include <memory>
#include "resource_function/expansion/trivial_expansion_function.h"
#include "resource_function/feasibility/trivial_feasibility_function.h"
#include "resource_function/cost/trivial_cost_function.h"
#include "resource_function/dominance/trivial_dominance_function.h"


Resource::Resource() :
  expansion_function_(std::make_unique<TrivialExpansionFunction>()), 
  feasibility_function_(std::make_unique<TrivialFeasibilityFunction>()),
  cost_function_(std::make_unique<TrivialCostFunction>()), 
  dominance_function_(std::make_unique<TrivialDominanceFunction>()) {

}

Resource::Resource(std::unique_ptr<ExpansionFunction> expansion_function, std::unique_ptr<FeasibilityFunction> feasibility_function,
  std::unique_ptr<CostFunction> cost_function, std::unique_ptr<DominanceFunction> dominance_function) :
  expansion_function_(std::move(expansion_function)), feasibility_function_(std::move(feasibility_function)),
  cost_function_(std::move(cost_function)), dominance_function_(std::move(dominance_function)) {

}

Resource::Resource(Resource const& rhs_resource) :
  expansion_function_(rhs_resource.expansion_function_->clone()),
  feasibility_function_(rhs_resource.feasibility_function_->clone()),
  cost_function_(rhs_resource.cost_function_->clone()),
  dominance_function_(rhs_resource.dominance_function_->clone()) {

}

Resource::~Resource() {

}

Resource& Resource::operator=(Resource const& rhs_resource) {
  expansion_function_ = rhs_resource.expansion_function_->clone();
  feasibility_function_ = rhs_resource.feasibility_function_->clone();
  cost_function_ = rhs_resource.cost_function_->clone();
  dominance_function_ = rhs_resource.dominance_function_->clone();

  return *this;
}

bool Resource::operator<=(const Resource& rhs_resource) const {

  return dominance_function_->check_dominance(*this, rhs_resource);
}

void Resource::expand(const Resource& rhs_resource, Resource& expanded_resource) const {

  expansion_function_->expand(*this, rhs_resource, expanded_resource);
}

double Resource::get_cost() const {

  return cost_function_->get_cost(*this);
}

bool Resource::is_feasible() const {

  return feasibility_function_->is_feasible(*this);
}
