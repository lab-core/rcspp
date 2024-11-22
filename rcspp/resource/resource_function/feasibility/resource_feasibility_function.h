#pragma once

#include "resource/resource_function/feasibility/feasibility_function.h"
#include "general/clonable.h"

#include <stdexcept>


template<class DerivedFeasibilityFunction, class DerivedResource>
class ResourceFeasibilityFunction : public Clonable< DerivedFeasibilityFunction, FeasibilityFunction> {
public:

  bool is_feasible(const Resource& resource) override {

    auto derived_resource = dynamic_cast<DerivedResource const*>(&resource);

    if (!derived_resource) {
      throw std::invalid_argument("The Resource arguments is not of the right type!");
    }

    return is_feasible(*derived_resource);
  }

  virtual bool is_feasible(const DerivedResource& resource) = 0;
};