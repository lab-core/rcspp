#pragma once

#include "resource/resource_function/expansion/resource_expansion_function.h"
#include "resource/concrete/real_resource.h"


class RealAdditionExpansionFunction : public ResourceExpansionFunction<RealAdditionExpansionFunction, RealResource> {

  void expand(const RealResource& lhs_resource, const RealResource& rhs_resource, 
    RealResource& expanded_resource) override;

};