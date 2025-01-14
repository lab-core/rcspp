#pragma once

//#include "resource/resource.h"

#include <memory>


template <typename ResourceType>
class ExpansionFunction {

public:

  virtual ~ExpansionFunction() {}

  virtual void expand(const ResourceType& cumul_resource, const ResourceType& rhs_resource, ResourceType& expanded_resource) = 0;

  virtual std::unique_ptr<ExpansionFunction> clone() const = 0;
};