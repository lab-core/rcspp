#pragma once

#include "resource/resource_function/dominance/dominance_function.h"
#include "general/clonable.h"


class TrivialDominanceFunction : public Clonable<TrivialDominanceFunction, DominanceFunction> {

public:
  bool check_dominance(const Resource& lhs_resource, const Resource& rhs_resource) override;

};