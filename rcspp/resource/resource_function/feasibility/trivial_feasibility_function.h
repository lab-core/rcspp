#pragma once

#include "resource/resource.h"
#include "resource/resource_function/feasibility/feasibility_function.h"
#include "general/clonable.h"


class TrivialFeasibilityFunction : public Clonable<TrivialFeasibilityFunction, FeasibilityFunction> {

public:
  bool is_feasible(const Resource& resource) override;
};