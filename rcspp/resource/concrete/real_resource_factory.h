#pragma once

#include "resource/resource_factory.h"
#include "real_resource.h"


class RealResourceFactory : public ResourceFactory<RealResource> {

public:

  RealResourceFactory();

  RealResourceFactory(std::unique_ptr<ExpansionFunction<RealResource>> expansion_function,
    std::unique_ptr<FeasibilityFunction<RealResource>> feasibility_function,
    std::unique_ptr<CostFunction<RealResource>> cost_function,
    std::unique_ptr<DominanceFunction<RealResource>> dominance_function);

  RealResourceFactory(std::unique_ptr<RealResource> resource_prototype);

  std::unique_ptr<RealResource> make_resource();

  std::unique_ptr<RealResource> make_resource(double value);

  std::unique_ptr<RealResource> make_resource(double value, double min, double max);

};
