#pragma once

#include "resource/resource_function/expansion/expansion_function.h"
#include "general/clonable.h"

#include <stdexcept>


template<class DerivedExpansionFunction, class DerivedResource>
class ResourceExpansionFunction : public Clonable<DerivedExpansionFunction, ExpansionFunction> {
public:

  void expand(const Resource& lhs_resource, const Resource& rhs_resource, 
    Resource& expanded_resource) override {

    auto lhs_derived_resource = dynamic_cast<DerivedResource const*>(&lhs_resource);
    auto rhs_derived_resource = dynamic_cast<DerivedResource const*>(&rhs_resource);
    auto expanded_derived_resource = dynamic_cast<DerivedResource*>(&expanded_resource);

    if (!lhs_derived_resource || !rhs_derived_resource || !expanded_derived_resource) {
      throw std::invalid_argument("The Resource arguments is not of the right type!");
    }

    expand(*lhs_derived_resource, *rhs_derived_resource, *expanded_derived_resource);
  }

  virtual void expand(const DerivedResource& lhs_derived_resource,
    const DerivedResource& rhs_derived_resource, DerivedResource& expanded_derived_resource) = 0;
};