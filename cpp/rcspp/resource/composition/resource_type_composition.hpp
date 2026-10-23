// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <stdexcept>
#include <string>

#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

/// @brief Tag type that bundles multiple resource types into a single composition type.
///
/// `ResourceTypeComposition` carries no runtime data; it exists solely as a compile-time
/// tag that groups @p ResourceTypes together so that the `Resource`, `Extender`, and
/// factory templates can be specialised for composed resources.
///
/// `get_value()` returns a reference to `*this` so that the composition satisfies the
/// `ComponentInitializerTypeTuple` trait.  Calling `set_value()` is a logic error because
/// the composition type has no scalar value to assign.
///
/// @tparam ResourceTypes The individual resource types forming the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ResourceTypeComposition {
    public:
        /// @brief Default constructor.
        ResourceTypeComposition() = default;

        /// @brief No-op reset — the composition tag carries no state.
        void reset() {}

        /// @brief Returns a const reference to this composition tag (satisfies value-trait).
        ///
        /// @return Const reference to `*this`.
        [[nodiscard]] const ResourceTypeComposition& get_value() const { return *this; }

        /// @brief Always throws — a composition tag has no settable scalar value.
        ///
        /// @tparam Args Argument types (ignored).
        /// @throws std::logic_error Always.
        template <typename... Args>
        void set_value(Args&&... /* args */) {
            throw std::logic_error("ResourceTypeComposition::set_value(...) is not available");
        }

        /// @brief Returns an empty string (the composition has no scalar representation).
        ///
        /// @return An empty `std::string`.
        [[nodiscard]] std::string to_string() const { return ""; }
};

}  // namespace rcspp
