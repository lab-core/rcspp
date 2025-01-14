#pragma once

#include "resource/resource_function/expansion/expansion_function.h"
#include "resource/concrete/real_resource.h"
#include "general/clonable.h"


class RealAdditionExpansionFunction : public Clonable<RealAdditionExpansionFunction, ExpansionFunction<RealResource>> {

  void expand(const RealResource& lhs_resource, const RealResource& rhs_resource, 
    RealResource& expanded_resource) override;

};