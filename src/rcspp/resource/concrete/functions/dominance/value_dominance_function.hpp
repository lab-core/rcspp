// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

template <typename ResourceType>
class ValueDominanceFunction
    : public Clonable<ValueDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {
    public:
        using ValueType =
            std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>;

        auto check_dominance(const Resource<ResourceType>& lhs_resource,
                             const Resource<ResourceType>& rhs_resource) -> bool override {
            return lhs_resource.leq(rhs_resource.get_value());
        }
};
}  // namespace rcspp
