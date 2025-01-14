#pragma once

//#include "resource/resource.h"

#include <memory>


template <typename ResourceType>
class DominanceFunction {

public:

  virtual ~DominanceFunction() {}

  virtual bool check_dominance(const ResourceType& lhs_resource, const ResourceType& rhs_resource) = 0;

  virtual std::unique_ptr<DominanceFunction> clone() const = 0;
};