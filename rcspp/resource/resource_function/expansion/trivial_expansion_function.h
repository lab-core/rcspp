#pragma once

#include "resource/resource_function/expansion/expansion_function.h"
#include "general/clonable.h"


class TrivialExpansionFunction : public Clonable<TrivialExpansionFunction, ExpansionFunction> {

public:
  void expand(const Resource& lhs_resource, const Resource& rhs_resource, 
    Resource& reused_resource) override;

};