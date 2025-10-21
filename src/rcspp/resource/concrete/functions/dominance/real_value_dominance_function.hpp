// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

class RealValueDominanceFunction
    : public Clonable<RealValueDominanceFunction, DominanceFunction<RealResource>> {
    public:
        auto check_dominance(const Resource<RealResource>& lhs_resource,
                             const Resource<RealResource>& rhs_resource) -> bool override;
};
}  // namespace rcspp