// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/functions/feasibility/composition_feasibility_function.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ReachableResourceType, typename... ResourceTypes>
    requires ResourceTypeConcept<ReachableResourceType> &&
             (ResourceTypeConcept<ResourceTypes> && ...)
class ReachableCompositionFeasibilityFunction
    : public Clonable<
          ReachableCompositionFeasibilityFunction<ReachableResourceType, ResourceTypes...>,
          CompositionFeasibilityFunction<ResourceTypes...>,
          FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        explicit ReachableCompositionFeasibilityFunction(size_t reachable_resource_index)
            : reachable_resource_index_(reachable_resource_index) {}

        bool is_reachable(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition,
            size_t destination_node_id) override {
            const auto& reachable_resource_ =
                resource_composition.template get_component<ReachableResourceType>(
                    reachable_resource_index_);
            return reachable_resource_.is_reachable(destination_node_id);
        }

    protected:
        size_t reachable_resource_index_;
};
}  // namespace rcspp
