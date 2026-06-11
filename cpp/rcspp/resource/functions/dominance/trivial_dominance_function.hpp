// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"

namespace rcspp {

/// @brief Dominance function that considers every label as dominated by every other.
///
/// Both @c check_dominance() and @c fast_check_dominance() unconditionally return
/// @c true, meaning no label is ever kept when a competing label exists. This is
/// useful as a placeholder or for algorithms that do not require dominance pruning.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
class TrivialDominanceFunction
    : public Clonable<TrivialDominanceFunction<ResourceType>, DominanceFunction<ResourceType>> {
    public:
        /// @brief Always returns @c true, indicating @p lhs_resource is dominated.
        ///
        /// @param lhs_resource The resource value being tested (unused).
        /// @param rhs_resource The reference resource value (unused).
        /// @return @c true unconditionally.
        [[nodiscard]] bool check_dominance(const ResourceType& lhs_resource,
                                           const ResourceType& rhs_resource) override {
            return true;
        }

        // clang-format off
        /// @brief Always returns @c true for the fast dominance check.
        ///
        /// @param lhs_resource The resource value being tested (unused).
        /// @param rhs_resource The reference resource value (unused).
        /// @param delta        Tolerance value (unused).
        /// @return @c true unconditionally.
        // Use to check (partial) dominance quickly. Useful for more complex data structure
        bool fast_check_dominance(const ResourceType& lhs_resource,
                                  const ResourceType& rhs_resource,
                                  double delta) override {
            return true;
        }
        // clang-format on
};
}  // namespace rcspp
