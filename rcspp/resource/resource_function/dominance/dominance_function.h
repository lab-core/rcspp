#pragma once

#include "resource/resource.h"


class DominanceFunction {

public:

  virtual ~DominanceFunction() {}

  virtual bool check_dominance(const Resource& lhs_resource, const Resource& rhs_resource) = 0;

  virtual std::unique_ptr<DominanceFunction> clone() const = 0;
};