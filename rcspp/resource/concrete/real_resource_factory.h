#pragma once

#include "resource/concrete_resource_factory.h"
#include "real_resource.h"


class RealResourceFactory : public ConcreteResourceFactory<RealResource> {

public:

  std::unique_ptr<RealResource> make_resource(double value);

  std::unique_ptr<RealResource> make_resource(double value, double min, double max);
};
