#pragma once

#include "resource/resource_function/cost/cost_function.h"
#include "resource/resource.h"
#include "general/clonable.h"


class TrivialCostFunction : public Clonable<TrivialCostFunction, CostFunction> {

public:
  double get_cost(const Resource& resource) const override;

};