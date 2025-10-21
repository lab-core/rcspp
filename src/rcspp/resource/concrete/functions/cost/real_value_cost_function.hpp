// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

class RealValueCostFunction : public Clonable<RealValueCostFunction, CostFunction<RealResource>> {
    public:
        [[nodiscard]] auto get_cost(const Resource<RealResource>& real_resource) const
            -> double override;
};
}  // namespace rcspp