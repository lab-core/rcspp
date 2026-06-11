// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

/// @brief Dominance function based on scalar value ordering: the left-hand resource
///        dominates the right-hand resource when its value is less than or equal to the
///        right-hand resource's value.
///
/// A label `lhs` dominates label `rhs` if `lhs_resource.leq(rhs_resource)`, meaning
/// the forward resource accumulates a smaller (or equal) scalar cost.  This is the
/// standard dominance rule for scalar resources such as time, cost, or distance.
///
/// The fast dominance check uses a scalar comparison with a tolerance `delta`.
///
/// @tparam ResourceType The resource type whose value supports `leq()` and `get_value()`.
template <typename ResourceType>
class ValueDominanceFunction
    : public Clonable<ValueDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {
    public:
        /// @brief Convenience alias for the scalar type of the resource value.
        using ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>;

        /// @brief Checks whether `lhs_resource` dominates `rhs_resource` by scalar value.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @return `true` if `lhs_resource.leq(rhs_resource)`.
        [[nodiscard]] auto check_dominance(const ResourceType& lhs_resource,
                                           const ResourceType& rhs_resource) -> bool override {
            return lhs_resource.leq(rhs_resource);
        }

        /// @brief Performs a fast approximate dominance check with a scalar tolerance.
        ///
        /// Returns `true` when `lhs_resource.value <= rhs_resource.value + delta`.
        /// Useful for pruning with a small numerical relaxation without a full comparison.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @param delta Tolerance added to the right-hand value.
        /// @return `true` if `lhs_resource.leq(rhs_resource.get_value() + delta)`.
        // clang-format off
        auto fast_check_dominance(const ResourceType& lhs_resource,
                                  const ResourceType& rhs_resource, double delta)
            -> bool override {
            return lhs_resource.leq(rhs_resource.get_value() + delta);
        }
        // clang-format on
};
}  // namespace rcspp
