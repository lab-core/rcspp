// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/functions/extension/composition_extension_function.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function for composed resources that incorporates a reachability check.
///
/// Extends `CompositionExtensionFunction` with awareness of one designated
/// "reachable" sub-resource (identified by @p ReachableResourceType and
/// `reachable_resource_index_`) that carries per-node reachability information.
///
/// @note This class is not yet implemented; the constructor always throws
///       `std::runtime_error`.
///
/// @tparam ReachableResourceType The resource type that carries reachability information.
/// @tparam ResourceTypes         The individual resource types forming the composition.
template <typename ReachableResourceType, typename... ResourceTypes>
    requires ResourceTypeConcept<ReachableResourceType> &&
             (ResourceTypeConcept<ResourceTypes> && ...)
class ReachableCompositionExtensionFunction
    : public Clonable<ReachableCompositionExtensionFunction<ResourceTypes...>,
                      CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        /// @brief Constructs the function, designating the reachable sub-resource by index.
        ///
        /// @param reachable_resource_index Position of the reachable sub-resource in its type slot.
        /// @throws std::runtime_error Always — this class is not yet implemented.
        explicit ReachableCompositionExtensionFunction(size_t reachable_resource_index)
            : reachable_resource_index_(reachable_resource_index) {
            throw std::runtime_error("ReachableCompositionExtensionFunction: Not implemented");
        }

    protected:
        /// @brief Position of the reachable sub-resource within its type-slot vector.
        size_t reachable_resource_index_;

        /// @brief Checks whether all resources in @p sing_res_vec can reach @p node_id.
        ///
        /// @param sing_res_vec Vector of owning pointers to sub-resources to check.
        /// @param node_id      The destination node identifier.
        /// @return `true` if every sub-resource in @p sing_res_vec is reachable at @p node_id.
        bool check_reachability(const auto& sing_res_vec, size_t node_id) {
            for (auto&& res_comp : sing_res_vec) {
                if (!res_comp->is_reachable(node_id)) {
                    return false;
                }
            }
            return true;
        }
};
}  // namespace rcspp
