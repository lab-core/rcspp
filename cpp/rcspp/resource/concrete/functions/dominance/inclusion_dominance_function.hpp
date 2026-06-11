// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

/// @brief Dominance function based on inclusion: the left-hand resource dominates the
///        right-hand resource when the right-hand resource's container includes all
///        elements of the left-hand resource.
///
/// A label `lhs` dominates label `rhs` (i.e., `lhs <= rhs`) if `rhs_resource`
/// includes (is a superset of) `lhs_resource`.  This is the dual of
/// `ContainDominanceFunction`: a label is "smaller" (better) when it holds fewer
/// elements, and a label with fewer elements is dominated by a label that is a subset
/// of the other.
///
/// The fast dominance check approximates this via a size comparison.
///
/// @tparam ContainerResourceType The resource type whose value is a container supporting
///         `includes()` and `size()`.
template <typename ContainerResourceType>
class InclusionDominanceFunction
    : public Clonable<InclusionDominanceFunction<ContainerResourceType>,
                      DominanceFunction<ContainerResourceType>> {
    public:
        /// @brief Checks whether `lhs_resource` dominates `rhs_resource` by inclusion.
        ///
        /// `lhs_resource` dominates `rhs_resource` if `rhs_resource` includes all elements of
        /// `lhs_resource`'s container value (i.e., `lhs_resource` is a subset of
        /// `rhs_resource`).
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @return `true` if `rhs_resource.includes(lhs_resource.get_value())`.
        [[nodiscard]] auto check_dominance(const ContainerResourceType& lhs_resource,
                                           const ContainerResourceType& rhs_resource)
            -> bool override {
            // lhs_resource dominates rhs_resource if lhs_resource <= rhs_resource
            // i.e., if rhs_resource includes lhs_resource
            return rhs_resource.includes(lhs_resource.get_value());
        }

        /// @brief Performs a fast approximate dominance check using container sizes.
        ///
        /// Returns `true` when `lhs_resource.size() <= rhs_resource.size() + delta`,
        /// which is a necessary condition for `rhs_resource` to include `lhs_resource`.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @param delta Tolerance added to the right-hand size for relaxed comparisons.
        /// @return `true` if `lhs_resource.size() <= rhs_resource.size() + delta`.
        // clang-format off
        auto fast_check_dominance(const ContainerResourceType& lhs_resource,
                                  const ContainerResourceType& rhs_resource, double delta)
            -> bool override {
            return lhs_resource.size() <= rhs_resource.size() + delta;
        }
        // clang-format on
};
}  // namespace rcspp
