#pragma once

#include "resource/resource_function/dominance/resource_dominance_function.h"
#include "resource/concrete/real_resource.h"


class RealValueDominanceFunction : public ResourceDominanceFunction<RealValueDominanceFunction, RealResource> {

  bool check_dominance(const RealResource& lhs_resource, const RealResource& rhs_resource) override;

};