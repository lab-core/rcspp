// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <set>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

// Deduce the container/value type by calling get_value() on the concrete Resource
template <typename ResourceType>
class ReachableFeasibilityFunction : public Clonable<ReachableFeasibilityFunction<ResourceType>,
                                                     FeasibilityFunction<ResourceType>> {
    public:
        explicit ReachableFeasibilityFunction(const ResourceType* const checked_nodes)
            : checked_nodes_(checked_nodes) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override { return true; }

        auto is_reachable(const Resource<ResourceType>& resource, size_t node_id) -> bool override {
            // either not to be checked (i.e., not required) or contained in the reachable set
            return !checked_nodes_->contains(node_id) || resource.contains(node_id);
        }

    private:
        const ResourceType* const checked_nodes_;
};
}  // namespace rcspp
