#pragma once

//#include "resource/resource.h"

#include <memory>


template <typename ResourceType>
class CostFunction {

public:

  virtual ~CostFunction() {}

  virtual double get_cost(const ResourceType& resource) const = 0;

  virtual std::unique_ptr<CostFunction> clone() const = 0;
};