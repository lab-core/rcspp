#pragma once

#include "resource/resource_function/cost/resource_cost_function.h"
#include "resource/concrete/real_resource.h"


class RealValueCostFunction : public ResourceCostFunction<RealValueCostFunction, RealResource> {

public:
  virtual double get_cost(const RealResource& real_resource) const override;
};