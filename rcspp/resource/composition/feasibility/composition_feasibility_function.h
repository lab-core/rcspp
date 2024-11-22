#pragma once

#include "resource/resource_function/feasibility/resource_feasibility_function.h"
#include "resource/composition/resource_composition.h"


class CompositionFeasibilityFunction : public ResourceFeasibilityFunction<CompositionFeasibilityFunction, ResourceComposition> {

public:
  bool is_feasible(const ResourceComposition& resource) override;
};