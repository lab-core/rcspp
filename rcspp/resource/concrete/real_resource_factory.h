#pragma once

#include "resource/resource_factory.h"
#include "real_resource.h"


class RealResourceFactory : public ResourceFactory<RealResource> {

public:

  RealResourceFactory(std::unique_ptr<RealResource> resource_prototype) 
    : ResourceFactory<RealResource>(std::move(resource_prototype)) {}

  std::unique_ptr<RealResource> make_resource(double value);

  std::unique_ptr<RealResource> make_resource(double value, double min, double max);

};
