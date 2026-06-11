// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

/// @brief Dominance function based on containment: the left-hand resource dominates the
///        right-hand resource when it includes all elements of the right-hand resource's
///        container value.
///
/// A label `lhs` is considered to dominate label `rhs` (i.e., `lhs <= rhs`) if
/// `lhs_resource` contains every element in `rhs_resource`.  Intuitively, a label
/// that has already visited a superset of nodes is "better" because it satisfies more
/// future reachability requirements.
///
/// The fast dominance check approximates this via a size comparison, using a tolerance
/// `delta` to account for relaxation.
///
/// @tparam ResourceType The resource type whose value is a container supporting
///         `includes()` and `size()`.
template <typename ResourceType>
class ContainDominanceFunction
    : public Clonable<ContainDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {
    public:
        /// @brief Checks whether `lhs_resource` dominates `rhs_resource` by containment.
        ///
        /// `lhs_resource` dominates `rhs_resource` if `lhs_resource` includes (contains) all
        /// elements in `rhs_resource`'s container value.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @return `true` if `lhs_resource.includes(rhs_resource.get_value())`.
        // clang-format off
        auto check_dominance(const ResourceType& lhs_resource, const ResourceType& rhs_resource)
            -> bool override {
            // lhs_resource dominates rhs_resource if lhs_resource <= rhs_resource
            // i.e., if lhs_resource contains rhs_resource
            return lhs_resource.includes(rhs_resource.get_value());
        }
        // clang-format on

        /// @brief Performs a fast approximate dominance check using container sizes.
        ///
        /// Returns `true` when `rhs_resource.size() <= lhs_resource.size() + delta`,
        /// which is a necessary (but not sufficient) condition for containment.  Useful for
        /// quickly pruning dominated labels without the cost of a full containment test.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @param delta Tolerance added to the left-hand size for relaxed comparisons.
        /// @return `true` if `rhs_resource.size() <= lhs_resource.size() + delta`.
        // clang-format off
        auto fast_check_dominance(const ResourceType& lhs_resource,
                                  const ResourceType& rhs_resource, double delta)
            -> bool override {
          return rhs_resource.size() <= lhs_resource.size() + delta;
        }
        // clang-format on
};
}  // namespace rcspp
