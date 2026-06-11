// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"

namespace rcspp {

/// @brief Cost function that always returns zero regardless of the resource value.
///
/// Useful as a placeholder when the objective is handled by another mechanism
/// (e.g., a separate arc cost), or when no cost needs to be tracked for a
/// particular resource component.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
class TrivialCostFunction
    : public Clonable<TrivialCostFunction<ResourceType>, CostFunction<ResourceType>> {
    public:
        /// @brief Returns zero cost unconditionally.
        ///
        /// @param resource The accumulated resource value (unused).
        /// @return @c 0.0 unconditionally.
        [[nodiscard]] double get_cost(const ResourceType& resource) const override { return 0; }
};
}  // namespace rcspp
