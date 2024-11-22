#include "real_resource_factory.h"

#include "resource/resource_function/expansion/expansion_function.h"
#include "resource/concrete/resource_function/feasibility/min_max_feasibility_function.h"
#include "resource/resource_function/cost/cost_function.h"
#include "resource/resource_function/dominance/dominance_function.h"


std::unique_ptr<RealResource> RealResourceFactory::make_resource(double value) {
  
  std::cout << "RealResourceFactory::make_resource(double value)" << std::endl;

  auto cloned_resource = concrete_resource_prototype_->concrete_clone();

  cloned_resource->set_value(value);

  return cloned_resource;
}

std::unique_ptr<RealResource> RealResourceFactory::make_resource(double value, double min, double max) {

  std::cout << "RealResourceFactory::make_resource(double value, double min, double max)" << std::endl;
  
  auto min_max_feasibility_function = std::make_unique<MinMaxFeasibilityFunction>(min, max);

  auto new_resource = std::make_unique<RealResource>(std::move(concrete_resource_prototype_->expansion_function_->clone()),
    std::move(min_max_feasibility_function),
    std::move(concrete_resource_prototype_->cost_function_->clone()),
    std::move(concrete_resource_prototype_->dominance_function_->clone()), value);

  new_resource->set_value(value);

  return new_resource;
}