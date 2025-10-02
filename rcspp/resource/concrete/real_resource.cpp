#include "real_resource.h"
#include "resource/resource.h"

#include "resource/resource_function/feasibility/trivial_feasibility_function.h"
#include "resource_function/cost/real_value_cost_function.h"
#include "resource_function/dominance/real_value_dominance_function.h"
#include "resource_function/expansion/real_addition_expansion_function.h"
#include "resource_function/feasibility/min_max_feasibility_function.h"

RealResource::RealResource()
    : Resource<RealResource>(
          std::make_unique<RealAdditionExpansionFunction>(),
          std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
          std::make_unique<RealValueCostFunction>(),
          std::make_unique<RealValueDominanceFunction>()),
      value_(0) {}

RealResource::RealResource(
    std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function)
    : Resource<RealResource>(
          std::move(expansion_function), std::move(feasibility_function),
          std::move(cost_function), std::move(dominance_function)),
      value_(0) {}

RealResource::RealResource(double value)
    : Resource<RealResource>(
          std::make_unique<RealAdditionExpansionFunction>(),
          std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
          std::make_unique<RealValueCostFunction>(),
          std::make_unique<RealValueDominanceFunction>()),
      value_(value) {}

RealResource::RealResource(double value, double min, double max)
    : Resource<RealResource>(
          std::make_unique<RealAdditionExpansionFunction>(),
          std::make_unique<MinMaxFeasibilityFunction>(min, max),
          std::make_unique<RealValueCostFunction>(),
          std::make_unique<RealValueDominanceFunction>()),
      value_(value) {}

RealResource::RealResource(
    double value,
    std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function)
    : Resource<RealResource>(
          std::move(expansion_function), std::move(feasibility_function),
          std::move(cost_function), std::move(dominance_function)),
      value_(value) {}

double RealResource::get_value() const { return value_; }

void RealResource::set_value(double value) { value_ = value; }
