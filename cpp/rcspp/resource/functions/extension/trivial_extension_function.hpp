// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief No-op extension function that leaves the resource unchanged.
///
/// Useful as a placeholder when a resource does not need to accumulate
/// any value along arcs (e.g., pure counting or cost-only resources).
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
class TrivialExtensionFunction
    : public Clonable<TrivialExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
    public:
        /// @brief Performs no extension; leaves @p reused_resource unchanged.
        ///
        /// @param resource        The current accumulated resource value (unused).
        /// @param extender_value  The arc's contribution (unused).
        /// @param reused_resource Pointer to the result (not modified).
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* reused_resource) override {}
};
}  // namespace rcspp
