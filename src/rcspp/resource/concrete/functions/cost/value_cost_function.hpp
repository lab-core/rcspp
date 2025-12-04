// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

template <typename ResourceType>
class ValueCostFunction
    : public Clonable<ValueCostFunction<ResourceType>, CostFunction<ResourceType>> {
    public:
        [[nodiscard]] auto get_cost(const Resource<ResourceType>& num_resource) const
            -> double override {
            return num_resource.get_value();
        }
};
}  // namespace rcspp
