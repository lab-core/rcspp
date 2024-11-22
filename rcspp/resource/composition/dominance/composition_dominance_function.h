#pragma once

#include "resource/resource_function/dominance/resource_dominance_function.h"
#include "resource/composition/resource_composition.h"


class CompositionDominanceFunction : public ResourceDominanceFunction<CompositionDominanceFunction, ResourceComposition> {

  bool check_dominance(const ResourceComposition& lhs_resource, const ResourceComposition& rhs_resource) override;

};