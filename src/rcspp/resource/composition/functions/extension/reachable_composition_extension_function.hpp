// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <tuple>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/functions/extension/composition_extension_function.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ReachableResourceType, typename... ResourceTypes>
class ReachableCompositionExtensionFunction
    : public Clonable<ReachableCompositionExtensionFunction<ResourceTypes...>,
                      CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceComposition<ResourceTypes...>>> {
    public:
        explicit ReachableCompositionExtensionFunction(size_t reachable_resource_index)
            : reachable_resource_index_(reachable_resource_index) {}

    protected:
        size_t reachable_resource_index_;

        // TODO(antoine): to improve to be automatic
        void post_extend(
            const Resource<ResourceComposition<ResourceTypes...>>& resource,
            const Extender<ResourceComposition<ResourceTypes...>>& extender,
            Resource<ResourceComposition<ResourceTypes...>>* extended_resource) override {
            auto& extended_reachable_resource_ =
                extended_resource->template get_resource_component<ReachableResourceType>(
                    reachable_resource_index_);
            // // get an empty copy of same type
            // ReachableResourceType new_reachable_nodes;
            // for (auto node_id : extended_reachable_resource_.iterable()) {
            //     if (extended_resource->is_reachable(node_id)) {
            //         new_reachable_nodes.add(node_id);
            //     }
            // }
            // // set the new reachable nodes
            // extended_reachable_resource_.set_value(new_reachable_nodes.get_value());
        }

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
