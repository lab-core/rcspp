#pragma once

#include "resource/resource_function/dominance/dominance_function.h"
#include "general/clonable.h"

#include <stdexcept>


template<class DerivedDominanceFunction, class DerivedResource>
class ResourceDominanceFunction : public Clonable< DerivedDominanceFunction, DominanceFunction> {
public:

  bool check_dominance(const Resource& lhs_resource, const Resource& rhs_resource) override {

    auto lhs_derived_resource = dynamic_cast<DerivedResource const*>(&lhs_resource);
    auto rhs_derived_resource = dynamic_cast<DerivedResource const*>(&rhs_resource);

    if (!lhs_derived_resource || !rhs_derived_resource) {
      throw std::invalid_argument("The Resource arguments is not of the right type!");
    }

    return check_dominance(*lhs_derived_resource, *rhs_derived_resource);
  }

  virtual bool check_dominance(const DerivedResource& lhs_derived_resource,
    const DerivedResource& rhs_derived_resource) = 0;
};