// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

template <size_t ResourceTypeIndex, typename... ResourceTypes>
class ComponentDominanceFunction
    : public Clonable<ComponentDominanceFunction<ResourceTypeIndex, ResourceTypes...>,
                      DominanceFunction<ResourceComposition<ResourceTypes...>>> {
  public:
    explicit ComponentDominanceFunction(size_t resource_index) : resource_index_(resource_index) {}

    bool check_dominance(
      const Resource<ResourceComposition<ResourceTypes...>>& lhs_resource,
      const Resource<ResourceComposition<ResourceTypes...>>& rhs_resource) override {
      const auto& lhs_component_resource =
        lhs_resource.get_resource_component<ResourceTypeIndex>(resource_index_);
      const auto& rhs_component_resource =
        rhs_resource.get_resource_component<ResourceTypeIndex>(resource_index_);

      return lhs_component_resource <= rhs_component_resource;
    }

  private:
    size_t resource_index_;
};
}  // namespace rcspp
