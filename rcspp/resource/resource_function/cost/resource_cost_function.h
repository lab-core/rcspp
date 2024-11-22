#pragma once

#include "resource/resource_function/cost/cost_function.h"
#include "general/clonable.h"

#include <stdexcept>

#include <iostream>

template<class DerivedCostFunction, class DerivedResource>
class ResourceCostFunction : public Clonable< DerivedCostFunction, CostFunction> {
public:
  double get_cost(const Resource& resource) const override {

    auto derived_resource = dynamic_cast<DerivedResource const*>(&resource);
    if (!derived_resource) {
      throw std::invalid_argument("The Resource argument is not of the right type!");
    }

    return get_cost(*derived_resource);
  }

  virtual double get_cost(const DerivedResource& derived_resource) const = 0;
};