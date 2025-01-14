#include "real_resource_factory.h"

#include "resource/resource_function/expansion/expansion_function.h"
#include "resource/concrete/resource_function/feasibility/min_max_feasibility_function.h"
#include "resource/resource_function/cost/cost_function.h"
#include "resource/resource_function/dominance/dominance_function.h"

#include <iostream>
#include <memory>


std::unique_ptr<RealResource> RealResourceFactory::make_resource(double value) {
  
  std::cout << "RealResourceFactory::make_resource(double value)" << std::endl;

  auto cloned_resource = resource_prototype_->clone();

  cloned_resource->set_value(value);

  return cloned_resource;
}

std::unique_ptr<RealResource> RealResourceFactory::make_resource(double value, double min, double max) {

  std::cout << "RealResourceFactory::make_resource(double value, double min, double max)" << std::endl;
  
  auto min_max_feasibility_function = std::make_unique<MinMaxFeasibilityFunction>(min, max);

  auto new_resource = std::make_unique<RealResource>(value,
    std::move(resource_prototype_->expansion_function_->clone()),
    std::move(min_max_feasibility_function),
    std::move(resource_prototype_->cost_function_->clone()),
    std::move(resource_prototype_->dominance_function_->clone()));

  new_resource->set_value(value);

  return new_resource;
}