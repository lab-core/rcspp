#include "real_resource_factory.h"

#include "resource/concrete/resource_function/cost/real_value_cost_function.h"
#include "resource/concrete/resource_function/dominance/real_value_dominance_function.h"
#include "resource/concrete/resource_function/expansion/real_addition_expansion_function.h"
#include "resource/concrete/resource_function/feasibility/min_max_feasibility_function.h"
#include "resource/resource_function/cost/cost_function.h"
#include "resource/resource_function/dominance/dominance_function.h"
#include "resource/resource_function/expansion/expansion_function.h"
#include "resource/resource_function/feasibility/trivial_feasibility_function.h"

#include <iostream>
#include <memory>

RealResourceFactory::RealResourceFactory() {}

RealResourceFactory::RealResourceFactory(
    std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function)
    : ResourceFactory<RealResource>(
          std::move(expansion_function), std::move(feasibility_function),
          std::move(cost_function), std::move(dominance_function)) {}

RealResourceFactory::RealResourceFactory(
    std::unique_ptr<RealResource> resource_prototype)
    : ResourceFactory<RealResource>(std::move(resource_prototype)) {}

std::unique_ptr<RealResource> RealResourceFactory::make_resource() {

  std::cout << "make_resource\n";

  /*auto new_resource = this->resource_prototype_->clone();

  return std::move(new_resource);*/

  return ResourceFactory<RealResource>::make_resource();
}

std::unique_ptr<RealResource> RealResourceFactory::make_resource(double value) {

  std::cout << "RealResourceFactory::make_resource(" << value << ")\n";

  auto cloned_resource = resource_prototype_->clone();

  cloned_resource->set_value(value);

  return cloned_resource;
}

std::unique_ptr<RealResource>
RealResourceFactory::make_resource(double value, double min, double max) {

  std::cout << "RealResourceFactory::make_resource(" << value << ", " << min
            << ", " << max << ")\n";

  auto min_max_feasibility_function =
      std::make_unique<MinMaxFeasibilityFunction>(min, max);

  auto new_resource = std::make_unique<RealResource>(
      value, std::move(resource_prototype_->expansion_function_->clone()),
      std::move(min_max_feasibility_function),
      std::move(resource_prototype_->cost_function_->clone()),
      std::move(resource_prototype_->dominance_function_->clone()));

  new_resource->set_value(value);

  return new_resource;
}
