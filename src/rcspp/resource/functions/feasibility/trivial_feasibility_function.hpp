// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// #include "resource/resource.hpp"
#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

template <typename ResourceType>
class TrivialFeasibilityFunction
    : public Clonable<TrivialFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        auto is_feasible([[maybe_unused]] const Resource<ResourceType>& resource) -> bool override {
            return true;
        }
};
