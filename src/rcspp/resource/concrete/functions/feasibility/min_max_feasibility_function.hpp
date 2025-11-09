// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType>
class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        MinMaxFeasibilityFunction(double min, double max) : min_(min), max_(max) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            bool feasible = true;

            if ((resource.get_value() < min_) || (resource.get_value() > max_)) {
                feasible = false;
            }

            return feasible;
        }

    private:
        double min_;
        double max_;
};
}  // namespace rcspp
