// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/composition/functions/feasibility/composition_feasibility_function.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ReachableResourceType, typename... ResourceTypes>
class ReachableCompositionFeasibilityFunction
    : public Clonable<
          ReachableCompositionFeasibilityFunction<ReachableResourceType, ResourceTypes...>,
          CompositionFeasibilityFunction<ResourceTypes...>,
          FeasibilityFunction<ResourceComposition<ResourceTypes...>>> {
    public:
        explicit ReachableCompositionFeasibilityFunction(size_t reachable_resource_index)
            : reachable_resource_index_(reachable_resource_index) {}

        bool is_reachable(
            const Resource<ResourceComposition<ResourceTypes...>>& resource_composition,
            size_t node_id) override {
            const auto& reachable_resource_ =
                resource_composition.template get_resource_component<ReachableResourceType>(
                    reachable_resource_index_);
            return reachable_resource_.is_reachable(node_id);
        }

        // bool is_reachable(
        //         const Resource<ResourceComposition<ResourceTypes...>>& resource_composition,
        //         size_t node_id) override {
        //         const auto& resource_components = resource_composition.get_resource_components();
        //
        //         return std::apply(
        //             [&](auto&&... args) {
        //                 // The && operator acts as a break in the fold expression.
        //                 (check_reachability(args, node_id) && ...);
        //             },
        //             resource_components);
        //     }

    private:
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
