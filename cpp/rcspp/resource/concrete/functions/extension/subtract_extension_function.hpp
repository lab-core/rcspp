// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function that extends a container resource by set subtraction.
///
/// The extended value is the set difference: elements in the current resource that
/// are NOT in the extender's container.  Typical use: removing visited or consumed
/// elements from an eligibility set as a path is extended.
///
/// @tparam ResourceType A ContainerResource-compatible type supporting `subtract()`
///                      and `set_value()`.
template <typename ResourceType>
class SubtractExtensionFunction
    : public Clonable<SubtractExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
    public:
        /// @brief Extends @p resource by subtracting @p extender_value, storing the result in
        /// @p extended_resource.
        ///
        /// @param resource           Current container resource of the label.
        /// @param extender_value     Arc's container resource (elements to remove).
        /// @param extended_resource  Output: receives the set-difference result.
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            auto difference = resource.subtract(extender_value.get_value());
            extended_resource->set_value(difference);
        }

        /// @brief A container resource whose arc value is direction-independent data.
        ///
        /// Unlike @c NgPathExtensionFunction, the arc's extender value here is user-supplied set
        /// data attached to the arc, not a node identity, so there is nothing to swap between
        /// directions: each half accumulates over its own arcs with the same formula and the two
        /// halves are reconciled at the join. The inherited @c extend_back is therefore already
        /// correct.
        ///
        /// @return @c BackwardKind::Mirror.
        [[nodiscard]] BackwardKind backward_kind() const override { return BackwardKind::Mirror; }
};
}  // namespace rcspp
