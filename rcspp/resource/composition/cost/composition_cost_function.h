#pragma once

#include "resource/resource_function/cost/resource_cost_function.h"
#include "resource/composition/resource_composition.h"


class CompositionCostFunction : public ResourceCostFunction<CompositionCostFunction, ResourceComposition> {

public:
  virtual double get_cost(const ResourceComposition& resource_composition) const override;
};