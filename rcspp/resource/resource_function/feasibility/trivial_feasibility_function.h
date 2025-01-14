#pragma once

//#include "resource/resource.h"
#include "resource/resource_function/feasibility/feasibility_function.h"
#include "general/clonable.h"


template<typename ResourceType>
class TrivialFeasibilityFunction : public Clonable<TrivialFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {

public:
  bool is_feasible(const ResourceType& resource) override {

    return true;
  }
};