#pragma once

#include "resource/resource.h"


class CostFunction {

public:

  virtual ~CostFunction() {}

  virtual double get_cost(const Resource& resource) const = 0;

  virtual std::unique_ptr<CostFunction> clone() const = 0;
};