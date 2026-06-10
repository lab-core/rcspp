// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

template <typename ContainerResourceType>
class InclusionDominanceFunction
    : public Clonable<InclusionDominanceFunction<ContainerResourceType>,
                      DominanceFunction<ContainerResourceType>> {
    public:
        [[nodiscard]] auto check_dominance(const ContainerResourceType& lhs_resource,
                                           const ContainerResourceType& rhs_resource)
            -> bool override {
            // lhs_resource dominates rhs_resource if lhs_resource <= rhs_resource
            // i.e., if rhs_resource includes lhs_resource
            return rhs_resource.includes(lhs_resource.get_value());
        }

        // clang-format off
        auto fast_check_dominance(const ContainerResourceType& lhs_resource,
                                  const ContainerResourceType& rhs_resource, double delta)
            -> bool override {
            return lhs_resource.size() <= rhs_resource.size() + delta;
        }
        // clang-format on
};
}  // namespace rcspp
