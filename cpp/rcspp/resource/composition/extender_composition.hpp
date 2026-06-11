// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/composition/composition.hpp"

namespace rcspp {

/// @brief Specialization of `Extender` for a composed resource type.
///
/// Combines `ExtenderPrototype` (which owns the top-level `ExtensionFunction`) with
/// `Composition<Extender, ResourceTypes...>` so that each constituent resource type
/// can have its own per-arc extender stored in the composition's component vectors.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class Extender<ResourceTypeComposition<ResourceTypes...>>
    : public ExtenderPrototype<Extender<ResourceTypeComposition<ResourceTypes...>>,
                               ResourceTypeComposition<ResourceTypes...>>,
      public Composition<Extender, ResourceTypes...> {
        using ResourceType = ResourceTypeComposition<ResourceTypes...>;
        using Prototype = ExtenderPrototype<Extender, ResourceType>;

    public:
        /// @brief Default constructor.
        Extender() = default;

        /// @brief Constructs an extender with a given extension function and arc identifier.
        ///
        /// @param extension_function Owning pointer to the extension function.
        /// @param arc_id             Identifier of the arc this extender is associated with.
        Extender(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                 const size_t arc_id)
            : Prototype(std::move(extension_function), arc_id) {}

        /// @brief Creates a deep copy of this extender adapted to a new arc.
        ///
        /// Clones the top-level extension function via `create(arc)` and recursively
        /// clones every constituent resource extender in the composition.
        ///
        /// @param arc The arc for which to create the cloned extender.
        /// @return A `std::unique_ptr` to the newly created `Extender`.
        [[nodiscard]] auto clone(const Arc<ResourceType>& arc) const -> auto {
            auto new_extender =
                std::make_unique<Extender>(this->extension_function_->create(arc), arc.id);
            this->apply(*new_extender, [&arc](const auto& extenders, auto& new_extenders) {
                for (const auto& extender : extenders) {
                    new_extenders.emplace_back(extender->clone(arc));
                }
            });

            return new_extender;
        }

        /// @brief Extends a resource in the forward direction along the arc.
        ///
        /// Delegates to the stored extension function.
        ///
        /// @param resource          The current resource state to extend from.
        /// @param extended_resource Output pointer to the resource state after extension.
        void extend(const Resource<ResourceType>& resource,
                    Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend(resource, *this, extended_resource);
        }

        /// @brief Extends a resource in the backward direction along the arc.
        ///
        /// Delegates to the stored extension function.
        ///
        /// @param resource          The current resource state to extend from.
        /// @param extended_resource Output pointer to the resource state after backward extension.
        void extend_back(const Resource<ResourceType>& resource,
                         Resource<ResourceType>* extended_resource) const {
            this->extension_function_->extend_back(resource, *this, extended_resource);
        }
};
}  // namespace rcspp
