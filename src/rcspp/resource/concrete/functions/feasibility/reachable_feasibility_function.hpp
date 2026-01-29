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
        auto is_feasible(const Resource<ResourceType>& resource) -> bool override { return true; }

        auto is_reachable(const Resource<ResourceType>& resource, size_t node_id) -> bool override {
            return resource.contains(node_id);
        }
};
}  // namespace rcspp
