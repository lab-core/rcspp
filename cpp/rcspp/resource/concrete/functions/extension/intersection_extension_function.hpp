// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function that extends a container resource by set intersection.
///
/// The extended value is the intersection of the current resource's container and the
/// extender's container.  Typical use: tracking which elements (e.g. customers, nodes)
/// are reachable or eligible along a path by narrowing the candidate set at each arc.
///
/// @tparam ContainerResourceType A ContainerResource-compatible type supporting
///                               `get_intersection()` and `set_value()`.
template <typename ContainerResourceType>
class IntersectionExtensionFunction
    : public Clonable<IntersectionExtensionFunction<ContainerResourceType>,
                      ExtensionFunction<ContainerResourceType>> {
    public:
        /// @brief Extends @p resource by intersecting it with @p extender_value, storing the
        /// result in @p extended_resource.
        ///
        /// @param resource           Current container resource of the label.
        /// @param extender_value     Arc's container resource (elements to intersect with).
        /// @param extended_resource  Output: receives the intersection result.
        void extend(const ContainerResourceType& resource,
                    const ContainerResourceType& extender_value,
                    ContainerResourceType* extended_resource) override {
            auto intersection_value = resource.get_intersection(extender_value.get_value());
            extended_resource->set_value(intersection_value);
        }
};
}  // namespace rcspp
