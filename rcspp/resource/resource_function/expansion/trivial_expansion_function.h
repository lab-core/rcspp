#pragma once

#include "resource/resource_function/expansion/expansion_function.h"
#include "general/clonable.h"


template<typename ResourceType>
class TrivialExpansionFunction : public Clonable<TrivialExpansionFunction<ResourceType>, ExpansionFunction<ResourceType>> {

public:
  void expand(const ResourceType& lhs_resource, const ResourceType& rhs_resource, ResourceType& reused_resource) override {

  }

};