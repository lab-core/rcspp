#pragma once

#include "resource/resource.h"


class FeasibilityFunction {

public:

  virtual ~FeasibilityFunction() {}

  virtual bool is_feasible(const Resource& resource) = 0;

  virtual std::unique_ptr<FeasibilityFunction> clone() const = 0;
};