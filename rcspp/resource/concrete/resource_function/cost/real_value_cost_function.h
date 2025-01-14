#pragma once

#include "resource/resource_function/cost/cost_function.h"
#include "resource/concrete/real_resource.h"
#include "general/clonable.h"


class RealValueCostFunction : public Clonable<RealValueCostFunction, CostFunction<RealResource>> {

public:
  double get_cost(const RealResource& real_resource) const override;
};