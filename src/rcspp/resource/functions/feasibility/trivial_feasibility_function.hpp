// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// #include "resource/resource.hpp"
#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType>
class TrivialFeasibilityFunction
    : public Clonable<TrivialFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        [[nodiscard]] auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return true;
        }

        [[nodiscard]] auto can_be_merged(const Resource<ResourceType>& resource,
                                         const Resource<ResourceType>& back_resource)
            -> bool override {
            return true;
        }
};
}  // namespace rcspp
