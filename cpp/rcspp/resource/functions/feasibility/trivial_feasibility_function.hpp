// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// #include "resource/resource.hpp"
#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that unconditionally accepts every resource value.
///
/// Useful as a placeholder when no constraint is imposed on a particular
/// resource component, or during algorithm prototyping.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
class TrivialFeasibilityFunction
    : public Clonable<TrivialFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        /// @brief Always returns @c true regardless of the resource value.
        ///
        /// @param resource The accumulated resource value (unused).
        /// @return @c true unconditionally.
        [[nodiscard]] auto is_feasible(const ResourceType& resource) -> bool override {
            return true;
        }

        /// @brief @c AlwaysTrue: an unconstrained resource never blocks a join.
        ///
        /// @return @c JoinRule::AlwaysTrue.
        [[nodiscard]] JoinRule join_rule() const override { return JoinRule::AlwaysTrue; }
};
}  // namespace rcspp
