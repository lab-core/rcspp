#pragma once

#include "resource/concrete_resource_factory.h"
#include "resource_composition.h"


class ResourceCompositionFactory : public ConcreteResourceFactory<ResourceComposition> {

public:

  std::unique_ptr<ResourceComposition> make_resource(std::vector<std::unique_ptr<Resource>>& resource_components);

};