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

template <typename ReachableResourceType, typename... ResourceTypes>
    requires ResourceTypeConcept<ReachableResourceType> &&
             (ResourceTypeConcept<ResourceTypes> && ...)
class ReachableCompositionExtensionFunction
    : public Clonable<ReachableCompositionExtensionFunction<ResourceTypes...>,
                      CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
    public:
        explicit ReachableCompositionExtensionFunction(size_t reachable_resource_index)
            : reachable_resource_index_(reachable_resource_index) {
            throw std::runtime_error(
                "ReachableCompositionExtensionFunction: Not implemented");  // GCOVR_EXCL_LINE
        }

    protected:
        size_t reachable_resource_index_;

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
