// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/resource/concrete/real_resource_factory.hpp"

#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/resource/concrete/functions/cost/real_value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/real_value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/expansion/real_addition_expansion_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"
#include "rcspp/resource/functions/expansion/expansion_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"

namespace rcspp {

RealResourceFactory::RealResourceFactory()
    : ResourceFactory<RealResource>(std::make_unique<RealAdditionExpansionFunction>(),
                                    std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                    std::make_unique<RealValueCostFunction>(),
                                    std::make_unique<RealValueDominanceFunction>(),
                                    RealResource(0)) {}

RealResourceFactory::RealResourceFactory(const RealResource& real_resource_prototype)
    : ResourceFactory<RealResource>(std::make_unique<RealAdditionExpansionFunction>(),
                                    std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                    std::make_unique<RealValueCostFunction>(),
                                    std::make_unique<RealValueDominanceFunction>(),
                                    real_resource_prototype) {}

RealResourceFactory::RealResourceFactory(
    std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function)
    : ResourceFactory<RealResource>(std::move(expansion_function), std::move(feasibility_function),
                                    std::move(cost_function), std::move(dominance_function),
                                    RealResource(0)) {}

RealResourceFactory::RealResourceFactory(
    const RealResource& real_resource_prototype,
    std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function)
    : ResourceFactory<RealResource>(std::move(expansion_function), std::move(feasibility_function),
                                    std::move(cost_function), std::move(dominance_function),
                                    real_resource_prototype) {}

// std::unique_ptr<RealResource> RealResourceFactory::make_resource() {
//
//   return ResourceFactory<RealResource>::make_resource();
// }
//
// std::unique_ptr<RealResource> RealResourceFactory::make_resource(double
// value) {
//
//   auto cloned_resource = resource_prototype_->clone();
//
//   cloned_resource->set_value(value);
//
//   return cloned_resource;
// }
//
// std::unique_ptr<RealResource> RealResourceFactory::make_resource(double
// value, double min, double max) {
//
//   auto min_max_feasibility_function =
//   std::make_unique<MinMaxFeasibilityFunction>(min, max);
//
//   auto new_resource = std::make_unique<RealResource>(value,
//     std::move(resource_prototype_->expansion_function_->clone()),
//     std::move(min_max_feasibility_function),
//     std::move(resource_prototype_->cost_function_->clone()),
//     std::move(resource_prototype_->dominance_function_->clone()));
//
//   new_resource->set_value(value);
//
//   return new_resource;
// }
}  // namespace rcspp
