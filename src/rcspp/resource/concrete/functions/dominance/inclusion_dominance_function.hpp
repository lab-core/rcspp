// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

template <typename ResourceType>
class InclusionDominanceFunction
    : public Clonable<InclusionDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {
    public:
        auto check_dominance(const Resource<ResourceType>& lhs_resource,
                             const Resource<ResourceType>& rhs_resource) -> bool override {
            return lhs_resource.includes(rhs_resource);
        }
};
}  // namespace rcspp
