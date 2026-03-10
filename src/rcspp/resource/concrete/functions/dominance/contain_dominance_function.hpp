// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

template <typename ContainerResourceType>
class ContainDominanceFunction : public Clonable<ContainDominanceFunction<ContainerResourceType>,
                                                 DominanceFunction<ContainerResourceType>> {
    public:
        auto check_dominance(const Resource<ContainerResourceType>& lhs_resource,
                             const Resource<ContainerResourceType>& rhs_resource) -> bool override {
            // lhs_resource dominates rhs_resource if lhs_resource contains rhs_resource
            return lhs_resource.includes(rhs_resource.get_value());
        }
};
}  // namespace rcspp
