#pragma once

#include "resource/resource_function/dominance/dominance_function.h"
#include "general/clonable.h"


template<typename ResourceType>
class TrivialDominanceFunction : public Clonable<TrivialDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {

public:
  bool check_dominance(const ResourceType& lhs_resource, const ResourceType& rhs_resource) override {

    return true;
  }

};