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
        using ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>;

        // GCOVR_EXCL_START (ValueDominanceFunction::check_dominance; not called in unit tests)
        [[nodiscard]] auto check_dominance(const ResourceType& lhs_resource,
                                           const ResourceType& rhs_resource) -> bool override {
            return lhs_resource.leq(rhs_resource);
        }
        // GCOVR_EXCL_STOP

        // clang-format off
        auto fast_check_dominance(const ResourceType& lhs_resource,
                                  const ResourceType& rhs_resource, double delta)
            -> bool override {
            return lhs_resource.leq(rhs_resource.get_value() + delta);
        }
        // clang-format on
};
}  // namespace rcspp
