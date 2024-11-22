#pragma once

#include "resource_factory.h"


template<class DerivedResource>
class ConcreteResourceFactory : public ResourceFactory {

public:

  ConcreteResourceFactory(std::unique_ptr<DerivedResource> resource_prototype) :
    ResourceFactory(std::move(resource_prototype)), 
    concrete_resource_prototype_(static_cast<DerivedResource*>(resource_prototype_.get())) {

  }

protected:

  DerivedResource* concrete_resource_prototype_;
};