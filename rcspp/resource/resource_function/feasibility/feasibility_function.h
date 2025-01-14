#pragma once

//#include "resource/resource.h"

#include <memory>


template <typename ResourceType>
class FeasibilityFunction {

public:

  virtual ~FeasibilityFunction() {}

  virtual bool is_feasible(const ResourceType& resource) = 0;

  virtual std::unique_ptr<FeasibilityFunction> clone() const = 0;
};