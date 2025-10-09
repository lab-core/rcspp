// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

template <typename ResourceType>
class TrivialCostFunction
    : public Clonable<TrivialCostFunction<ResourceType>, CostFunction<ResourceType>> {
    public:
        double get_cost(const Resource<ResourceType>& resource) const override { return 0; }
};
