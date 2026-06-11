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

        /// @brief Always returns @c true, indicating any pair of labels can be merged.
        ///
        /// @param resource      The forward label's resource value (unused).
        /// @param back_resource The backward label's resource value (unused).
        /// @return @c true unconditionally.
        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            return true;
        }
};
}  // namespace rcspp
