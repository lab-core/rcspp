// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename ResourceType>
class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        using ValueType =
            std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>;

        MinMaxFeasibilityFunction(ValueType min, ValueType max) : min_(min), max_(max) {}

        auto is_feasible(const Resource<ResourceType>& resource) -> bool override {
            return min_ <= resource.get_value() && resource.get_value() <= max_;
        }

    private:
        ValueType min_;
        ValueType max_;
};
}  // namespace rcspp
