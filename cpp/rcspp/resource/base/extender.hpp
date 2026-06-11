// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "rcspp/resource/base/extender_prototype.hpp"

namespace rcspp {

/// @brief Concrete arc extender that applies a typed extension function to a resource label.
///
/// `Extender` is the leaf of the `ExtenderPrototype` CRTP hierarchy.  One `Extender`
/// instance lives on each arc of the resource graph for each resource dimension.  When
/// the solver extends a label along an arc, it calls `extend` (forward direction) or
/// `extend_back` (backward direction) on every extender associated with that arc.
///
/// @tparam ResourceType The resource value type; must satisfy `ResourceTypeConcept`.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Extender : public ExtenderPrototype<Extender<ResourceType>, ResourceType> {
        using Prototype = ExtenderPrototype<Extender, ResourceType>;

    public:
        /// @brief Constructs an extender with a copied resource value and extension function.
        ///
        /// @param resource_value      Arc resource value (copied).
        /// @param extension_function  Owned extension function.
        /// @param arc_id              Identifier of the associated arc.
        Extender(const ResourceType& resource_value,
                 std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(resource_value, std::move(extension_function), arc_id) {}

        /// @brief Constructs an extender by unpacking a tuple into the resource-value constructor.
        ///
        /// @tparam Args               Argument types packed in the initialiser tuple.
        /// @param resource_initializer Tuple whose elements initialise the arc `ResourceType`.
        /// @param extension_function  Owned extension function.
        /// @param arc_id              Identifier of the associated arc.
        template <typename... Args>
        Extender(const std::tuple<Args...>& resource_initializer,
                 std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(resource_initializer, std::move(extension_function), arc_id) {}

        /// @brief Constructs a default-value extender with the given extension function.
        ///
        /// @param extension_function Owned extension function.
        /// @param arc_id             Identifier of the associated arc.
        Extender(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(std::move(extension_function), arc_id) {}

        /// @brief Creates a new extender cloned for a specific arc of a (possibly different)
        ///        resource graph.
        ///
        /// The extension function is re-created for the new arc via
        /// `ExtensionFunction::create(arc)`, so any arc-specific state is refreshed.
        ///
        /// @tparam GraphResourceType Resource type of the target graph arc.
        /// @param arc                Target arc for which the clone is created.
        /// @return A heap-allocated `Extender` bound to `arc`.
        template <typename GraphResourceType>
        [[nodiscard]] auto clone(const Arc<GraphResourceType>& arc) const
            -> std::unique_ptr<Extender> {
            return std::make_unique<Extender>(this->value_,
                                              this->extension_function_->create(arc),
                                              arc.id);
        }

        // Resource extension
        /// @brief Extends a label in the forward direction along the arc.
        ///
        /// Delegates to the stored `ExtensionFunction::extend`, passing the current
        /// resource value and the arc's resource value, and writing the result into
        /// `extended_resource`.
        ///
        /// @param resource          The label resource before extension.
        /// @param extended_resource Output resource that receives the extended value.
        void extend(const Resource<ResourceType>& resource,
                    Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend(resource.get_value(),
                                              this->value_,
                                              &extended_resource->get_value());
        }

        /// @brief Extends a label in the backward direction along the arc.
        ///
        /// Delegates to `ExtensionFunction::extend_back` for backward labelling.
        ///
        /// @param resource          The backward label resource before extension.
        /// @param extended_resource Output resource that receives the extended value.
        void extend_back(const Resource<ResourceType>& resource,
                         Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend_back(resource.get_value(),
                                                   this->value_,
                                                   &extended_resource->get_value());
        }

        /// @brief Returns a human-readable string representation of the arc resource value.
        ///
        /// @return String representation of the stored resource value.
        [[nodiscard]] std::string to_string() const { return this->value_.to_string(); }
};
}  // namespace rcspp
