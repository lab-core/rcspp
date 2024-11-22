#pragma once

#include "resource/concrete_resource.h"

#include <vector>
#include <memory>


class ResourceComposition : public ConcreteResource<ResourceComposition> {
  friend class ResourceCompositionFactory;

private:
  std::vector<std::unique_ptr<Resource>> resource_components_;

public:

  ResourceComposition(std::vector<std::unique_ptr<Resource>>& resource_components);

  ResourceComposition(std::unique_ptr<ExpansionFunction> expansion_function,
    std::unique_ptr<FeasibilityFunction> feasibility_function,
    std::unique_ptr<CostFunction> cost_function,
    std::unique_ptr<DominanceFunction> dominance_function, 
    std::vector<std::unique_ptr<Resource>>& resource_components);

  ResourceComposition(const ResourceComposition& rhs_resource_composition);

  ~ResourceComposition();

  ResourceComposition& operator=(const ResourceComposition& rhs_resource_composition);

  std::vector<std::unique_ptr<Resource>>& get_components();

  const std::vector<std::unique_ptr<Resource>>& get_components() const;

  //virtual std::unique_ptr<Resource> clone() const override;
};