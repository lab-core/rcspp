// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

/// @brief CRTP base class for arc extender objects used in label extension.
///
/// An `ExtenderPrototype` stores the arc-local resource value (e.g. consumption on that
/// arc) together with an `ExtensionFunction` that knows how to propagate a label's
/// resource along the arc.  The CRTP pattern allows the base to return `unique_ptr` to
/// the concrete derived type from `clone()` without virtual dispatch.
///
/// @tparam ExtenderClass The concrete derived class (CRTP parameter).
/// @tparam ResourceType  The resource value type; must satisfy `ResourceTypeConcept`.
template <typename ExtenderClass, typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ExtenderPrototype {
    public:
        /// @brief Default constructor.
        ///
        /// Initialises the resource value to its default, sets the extension function to
        /// `nullptr`, and the arc identifier to 0.
        ExtenderPrototype() : value_(), extension_function_(nullptr), arc_id_(0) {}

        /// @brief Constructs an extender with an explicit resource value and extension function.
        ///
        /// @param resource_value      Arc resource value (moved into the extender).
        /// @param extension_function  Owned extension function applied during label propagation.
        /// @param arc_id              Identifier of the arc this extender is associated with.
        ExtenderPrototype(ResourceType resource_value,
                          std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : value_(std::move(resource_value)),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        /// @brief Constructs an extender by unpacking a tuple into the resource-value constructor.
        ///
        /// The tuple elements are forwarded as individual constructor arguments to `ResourceType`.
        ///
        /// @tparam Args               Argument types packed in the tuple.
        /// @param resource_initializer Tuple whose elements initialise the `ResourceType`.
        /// @param extension_function  Owned extension function.
        /// @param arc_id              Identifier of the associated arc.
        template <typename... Args>
        ExtenderPrototype(const std::tuple<Args...>& resource_initializer,
                          std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : value_(std::apply(
                  [](auto&&... args) {  // unpack arguments
                      return ResourceType(std::forward<decltype(args)>(args)...);
                  },
                  resource_initializer)),
              extension_function_(std::move(extension_function)),
              arc_id_(arc_id) {}

        /// @brief Constructs a default-value extender with the given extension function.
        ///
        /// @param extension_function Owned extension function.
        /// @param arc_id             Identifier of the associated arc.
        ExtenderPrototype(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          const size_t arc_id)
            : value_(), extension_function_(std::move(extension_function)), arc_id_(arc_id) {}

        /// @brief Creates a deep copy of this extender.
        ///
        /// @return A heap-allocated clone of the concrete derived extender.
        [[nodiscard]] auto clone() const -> std::unique_ptr<ExtenderClass> {
            return std::make_unique<ExtenderClass>(downcast());
        }

        /// @brief Returns a const reference to the stored arc resource value.
        ///
        /// @return Const reference to the resource value.
        [[nodiscard]] auto get_value() const -> const ResourceType& { return value_; }

        /// @brief Returns a mutable reference to the stored arc resource value.
        ///
        /// @return Mutable reference to the resource value.
        [[nodiscard]] auto get_value() -> ResourceType& { return value_; }

        // Forward set_value calls to the stored value (only valid when ResourceType has set_value)
        /// @brief Forwards a value-setting call to the underlying arc resource-value object.
        ///
        /// Only valid when `ResourceType` itself exposes a `set_value` method.
        ///
        /// @tparam Args Argument types forwarded to `ResourceType::set_value`.
        /// @param args  Arguments forwarded to `ResourceType::set_value`.
        template <typename... Args>
        void set_value(Args&&... args) {
            value_.set_value(std::forward<Args>(args)...);
        }

        /// @brief Returns the identifier of the arc associated with this extender.
        ///
        /// @return Arc identifier.
        [[nodiscard]] auto get_arc_id() const -> size_t { return arc_id_; }

    protected:
        ResourceType value_;
        std::unique_ptr<ExtensionFunction<ResourceType>> extension_function_;

    private:
        const size_t arc_id_;

        [[nodiscard]] ExtenderClass& downcast() { return static_cast<ExtenderClass&>(*this); }

        [[nodiscard]] const ExtenderClass& downcast() const {
            return static_cast<ExtenderClass const&>(*this);
        }
};
}  // namespace rcspp
