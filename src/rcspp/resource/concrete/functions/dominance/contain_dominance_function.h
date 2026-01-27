// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

template <typename ResourceType>
class ContainDominanceFunction
    : public Clonable<ContainDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {
    public:
        auto check_dominance(const Resource<ResourceType>& lhs_resource,
                             const Resource<ResourceType>& rhs_resource) -> bool override {
            // lhs_resource dominates rhs_resource if lhs_resource >= rhs_resource
            return lhs_resource.includes(rhs_resource.get_value());
        }
};
}  // namespace rcspp
