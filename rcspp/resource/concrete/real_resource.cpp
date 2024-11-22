#include "resource/resource.h"
#include "real_resource.h"

#include "resource_function/cost/real_value_cost_function.h"
#include "resource_function/expansion/real_addition_expansion_function.h"
#include "resource/resource_function/feasibility/trivial_feasibility_function.h"
#include "resource_function/dominance/real_value_dominance_function.h"

#include <cstdarg>


RealResource::RealResource(double value) : ConcreteResource<RealResource>(
  std::make_unique<RealAdditionExpansionFunction>(), 
  std::make_unique<TrivialFeasibilityFunction>(),
  std::make_unique<RealValueCostFunction>(), 
  std::make_unique<RealValueDominanceFunction>()), value_(value) {
  
}

RealResource::RealResource(std::unique_ptr<ExpansionFunction> expansion_function,
  std::unique_ptr<FeasibilityFunction> feasibility_function,
  std::unique_ptr<CostFunction> cost_function, 
  std::unique_ptr<DominanceFunction> dominance_function, 
  double value) : ConcreteResource<RealResource>(
    std::move(expansion_function),
    std::move(feasibility_function), 
    std::move(cost_function), std::move(dominance_function)), 
  value_(value) {
}

double RealResource::get_value() const {
  return value_;
}

void RealResource::set_value(double value) {
  value_ = value;
}
