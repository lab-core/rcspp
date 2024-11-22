#pragma once

#include "resource/resource_function/expansion/resource_expansion_function.h"
#include "resource/composition/resource_composition.h"


class CompositionExpansionFunction : public ResourceExpansionFunction<CompositionExpansionFunction, ResourceComposition> {

  void expand(const ResourceComposition& lhs_resource, const ResourceComposition& rhs_resource, 
    ResourceComposition& expanded_resource) override;

};