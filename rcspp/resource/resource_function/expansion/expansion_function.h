#pragma once

#include "resource/resource.h"

#include <memory>


class ExpansionFunction {

public:

  virtual ~ExpansionFunction() {}

  virtual void expand(const Resource& lhs_resource, const Resource& rhs_resource, Resource& expanded_resource) = 0;

  virtual std::unique_ptr<ExpansionFunction> clone() const = 0;
};