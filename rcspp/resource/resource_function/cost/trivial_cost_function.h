#pragma once

#include "resource/resource_function/cost/cost_function.h"
//#include "resource/resource.h"
#include "general/clonable.h"


template<typename ResourceType>
class TrivialCostFunction : public Clonable<TrivialCostFunction<ResourceType>, CostFunction<ResourceType>> {

public:
  double get_cost(const ResourceType& resource) const override {
    return 0;
  }

};