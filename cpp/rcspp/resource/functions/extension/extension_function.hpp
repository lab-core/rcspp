// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Extender;

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Arc;

/// @brief Reads an extension function type's backward kind at compile time, from its static
///        @c kind, or @c Unspecified when it has none.
///
/// For checks that run before any object exists, such as the presets' coherence assertion.
template <typename T>
concept DeclaresBackwardKind = requires {
    { T::kind } -> std::convertible_to<BackwardKind>;
};

template <typename T>
struct BackwardKindOf {
        static constexpr BackwardKind value = BackwardKind::Unspecified;
};

template <DeclaresBackwardKind T>
struct BackwardKindOf<T> {
        static constexpr BackwardKind value = T::kind;
};

template <typename T>
inline constexpr BackwardKind backward_kind_of_v = BackwardKindOf<T>::value;

/// @brief Abstract base class defining the extension function for a resource type.
///
/// An extension function computes the new resource value after traversing an arc
/// during forward (or backward) label extension in the RCSPP algorithm.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ExtensionFunction {
    public:
        virtual ~ExtensionFunction() = default;

        /// @brief Extends a resource value along an arc in the forward direction.
        ///
        /// @param resource        The current accumulated resource value.
        /// @param extender_value  The arc's contribution to the resource.
        /// @param extended_resource Pointer to the result; must not be null.
        virtual void extend(const ResourceType& resource, const ResourceType& extender_value,
                            ResourceType* extended_resource) = 0;

        /// @brief How a bidirectional solve reads this resource backward.
        ///
        /// The solve derives from it the join test and the direction of backward dominance.
        /// @c Unspecified by default, which keeps forward-only functions valid but makes a
        /// bidirectional solve refuse the resource.
        ///
        /// @return The backward form declared by this extension function.
        [[nodiscard]] virtual BackwardKind backward_kind() const {
            return BackwardKind::Unspecified;
        }

        /// @brief Extends a value backward along an arc: from a label at the arc's destination to
        ///        one at its origin.
        ///
        /// The backward search of a bidirectional solve calls it, from the sinks. What the backward
        /// value means follows @c backward_kind():
        ///  - @c Accumulate (this default): the suffix's sum, by the same formula as @c extend;
        ///  - @c Threshold: the most the prefix may reach at the origin. It must invert @c extend,
        ///    `extend(x, arc) <= b  <=>  x <= extend_back(b, arc)`, and clamp down to the origin's
        ///    bound;
        ///  - @c ArcValue, @c ArcEndpoints: the set seen on the suffix.
        ///
        /// @p extender_value is the arc's consumption, the same as going forward. Where the label
        /// starts, at the sink, is @c start_back's.
        ///
        /// A @c Threshold must accept any value of its type: @c BackwardExtensionCheck reads its
        /// clamps by extending values beyond every bound (the infinities, or an integral type's
        /// extremes), forward and backward, and checks them against the feasibility function.
        ///
        /// @param resource        The current accumulated resource value.
        /// @param extender_value  The arc's contribution to the resource.
        /// @param extended_resource Pointer to the result; must not be null.
        virtual void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                                 ResourceType* extended_resource) {
            extend(resource, extender_value, extended_resource);
        }

        /// @brief Sets the value a backward label starts with at this arc's destination.
        ///
        /// A bidirectional solve starts its backward labels at the sinks, with the type default,
        /// and calls this on an arc entering each sink. Each backward step applies what the node
        /// it arrives at requires, but no step arrives at the sink, so a form whose labels must
        /// start elsewhere sets that start here. The default leaves the type default.
        ///
        /// Set @p resource without reading it, or leave it unchanged: setup calls it on values
        /// beyond every bound to see which start it sets.
        ///
        /// @param resource The backward label's value at the arc's destination.
        virtual void start_back(ResourceType* /*resource*/) {}

        /// @brief Creates a polymorphic copy of this extension function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ExtensionFunction> = 0;

        /// @brief Clones this function and preprocesses it for a specific arc.
        ///
        /// @tparam GraphResourceType The resource type used by the graph arc.
        /// @param arc The arc whose origin and destination nodes are used for preprocessing.
        /// @return A new extension function instance ready for use on @p arc.
        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc) -> std::unique_ptr<ExtensionFunction> {
            auto new_extension_function = clone();
            new_extension_function->preprocess(arc.origin->id, arc.destination->id);
            return new_extension_function;
        }

    protected:
        /// @brief Optional arc-specific preprocessing hook.
        ///
        /// Called by @c create() after cloning. Override to cache arc-dependent data.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        virtual void preprocess(size_t origin_id, size_t destination_id) {}
};

/// @brief Thrown by a composed function whose backward members (@c extend_back and @c start_back
///        here) are not overridden.
///
/// @c BackwardExtensionCheck calls each once before any search, and turns this exception into a
/// refusal that names the missing override.
class NoBackwardExtension : public std::logic_error {
    public:
        using std::logic_error::logic_error;
};

/// @brief Specialization of @c ExtensionFunction for composed resource types.
///
/// When @c ResourceType is a @c ResourceTypeComposition, the extension function
/// receives full @c Resource and @c Extender objects so it can access all
/// component values simultaneously.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
// Specialization for ResourceTypeComposition: extension functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ExtensionFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~ExtensionFunction() = default;

        /// @brief Extends a composed resource along an arc in the forward direction.
        ///
        /// @param resource         The current accumulated composed resource.
        /// @param extender         The arc's extender carrying all component contributions.
        /// @param extended_resource Pointer to the result; must not be null.
        virtual void extend(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Extender<ResourceTypeComposition<ResourceTypes...>>& extender,
            Resource<ResourceTypeComposition<ResourceTypes...>>* extended_resource) = 0;

        /// @brief Not read: a composition has no kind of its own. The model checks read
        ///        each component's @c backward_kind() instead.
        ///
        /// @return @c BackwardKind::Unspecified unless overridden.
        [[nodiscard]] virtual BackwardKind backward_kind() const {
            return BackwardKind::Unspecified;
        }

        /// @brief Extends a composed resource backward.
        ///
        /// The default throws @c NoBackwardExtension, so a forward-only composition still compiles
        /// and the model checks refuse it by name. @c CompositionExtensionFunction extends
        /// each component.
        ///
        /// @param resource         The current accumulated composed resource.
        /// @param extender         The arc's extender carrying all component contributions.
        /// @param extended_resource Pointer to the result; must not be null.
        /// @throws NoBackwardExtension unless overridden.
        virtual void extend_back(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            const Extender<ResourceTypeComposition<ResourceTypes...>>& /*extender*/,
            Resource<ResourceTypeComposition<ResourceTypes...>>* /*extended_resource*/) {
            throw NoBackwardExtension(
                "this composition extension function has no backward form: override "
                "extend_back, as CompositionExtensionFunction does");
        }

        /// @brief Sets the values a backward label starts with at the arc's destination.
        ///
        /// Not pure, for the same reason as @c extend_back; the default throws.
        /// @c CompositionExtensionFunction starts each component through its own extender.
        ///
        /// @param extender The arc's extender carrying all component contributions.
        /// @param resource The backward label at the arc's destination.
        /// @throws NoBackwardExtension unless overridden.
        virtual void start_back(
            const Extender<ResourceTypeComposition<ResourceTypes...>>& /*extender*/,
            Resource<ResourceTypeComposition<ResourceTypes...>>* /*resource*/) {
            throw NoBackwardExtension(
                "this composition extension function has no backward start: override "
                "start_back, as CompositionExtensionFunction does");
        }

        /// @brief Creates a polymorphic copy of this extension function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        /// @brief Clones this function and preprocesses it for a specific arc.
        ///
        /// @tparam GraphResourceType The resource type used by the graph arc.
        /// @param arc The arc whose origin and destination nodes are used for preprocessing.
        /// @return A new extension function instance ready for use on @p arc.
        template <typename GraphResourceType>
        auto create(const Arc<GraphResourceType>& arc)
            -> std::unique_ptr<ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_extension_function = clone();
            new_extension_function->preprocess(arc.origin->id, arc.destination->id);
            return new_extension_function;
        }

    protected:
        /// @brief Optional arc-specific preprocessing hook.
        ///
        /// Called by @c create() after cloning. Override to cache arc-dependent data.
        ///
        /// @param origin_id      Index of the arc's origin node.
        /// @param destination_id Index of the arc's destination node.
        virtual void preprocess(size_t origin_id, size_t destination_id) {}
};

}  // namespace rcspp
