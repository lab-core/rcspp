// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction, FeasibilityFunction<RealResource>> {
    public:
        MinMaxFeasibilityFunction(double min, double max) : min_(min), max_(max) {}

        auto is_feasible(const Resource<RealResource>& resource) -> bool override;

    private:
        double min_;
        double max_;
};
}  // namespace rcspp