// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/functions/feasibility/composition_feasibility_function.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function for composed resources that adds reachability support.
///
/// Extends `CompositionFeasibilityFunction` by overriding `is_reachable` to delegate
/// the check to a specific sub-resource of type @p ReachableResourceType.  The
/// sub-resource is identified by its position `reachable_resource_index_` in the
/// corresponding type-slot vector.
///
/// @tparam ReachableResourceType The resource type that carries reachability information.
/// @tparam ResourceTypes         The individual resource types forming the composition.
template <typename ReachableResourceType, typename... ResourceTypes>
    requires ResourceTypeConcept<ReachableResourceType> &&
             (ResourceTypeConcept<ResourceTypes> && ...)
class ReachableCompositionFeasibilityFunction
    : public Clonable<
          ReachableCompositionFeasibilityFunction<ReachableResourceType, ResourceTypes...>,
          CompositionFeasibilityFunction<ResourceTypes...>,
          FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        /// @brief Constructs the function, designating the reachable sub-resource by index.
        ///
        /// @param reachable_resource_index Position of the reachable sub-resource in the
        ///        @p ReachableResourceType type-slot vector.
        explicit ReachableCompositionFeasibilityFunction(size_t reachable_resource_index)
            : reachable_resource_index_(reachable_resource_index) {}

        /// @brief Returns `true` if the designated sub-resource can reach @p destination_node_id.
        ///
        /// Retrieves the sub-resource at `reachable_resource_index_` from the
        /// @p ReachableResourceType slot of @p resource_composition and delegates to its
        /// `is_reachable` method.
        ///
        /// @param resource_composition  The composed resource to query.
        /// @param destination_node_id   The node identifier to test reachability for.
        /// @return Result of `reachable_resource.is_reachable(destination_node_id)`.
        bool is_reachable(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource_composition,
            size_t destination_node_id) override {
            const auto& reachable_resource_ =
                resource_composition.template get_component<ReachableResourceType>(
                    reachable_resource_index_);
            return reachable_resource_.is_reachable(destination_node_id);
        }

    protected:
        /// @brief Position of the reachable sub-resource within its type-slot vector.
        size_t reachable_resource_index_;
};
}  // namespace rcspp
