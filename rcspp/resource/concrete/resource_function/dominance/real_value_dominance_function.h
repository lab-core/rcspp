#pragma once

#include "resource/resource_function/dominance/dominance_function.h"
#include "resource/concrete/real_resource.h"
#include "general/clonable.h"


class RealValueDominanceFunction : public Clonable<RealValueDominanceFunction, DominanceFunction<RealResource>> {

  bool check_dominance(const RealResource& lhs_resource, const RealResource& rhs_resource) override;

};