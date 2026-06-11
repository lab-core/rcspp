// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function that extends a container resource by set union.
///
/// The extended value is the union of the current resource's container and the
/// extender's container.  Typical use: accumulating visited nodes or collected
/// items along a path where the resource grows monotonically.
///
/// @tparam ContainerResourceType A ContainerResource-compatible type supporting
///                               `get_union()` and `set_value()`.
template <typename ContainerResourceType>
class UnionExtensionFunction : public Clonable<UnionExtensionFunction<ContainerResourceType>,
                                               ExtensionFunction<ContainerResourceType>> {
    public:
        /// @brief Extends @p resource by taking its union with @p extender_value, storing the
        /// result in @p extended_resource.
        ///
        /// @param resource           Current container resource of the label.
        /// @param extender_value     Arc's container resource (elements to add).
        /// @param extended_resource  Output: receives the union result.
        void extend(const ContainerResourceType& resource,
                    const ContainerResourceType& extender_value,
                    ContainerResourceType* extended_resource) override {
            auto union_value = resource.get_union(extender_value.get_value());
            extended_resource->set_value(union_value);
        }
};
}  // namespace rcspp
