// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
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

/// @brief True when an extension function publishes its backward kind as a static @c kind,
///        which must agree with its @c backward_kind() virtual.
template <typename T>
concept DeclaresBackwardKind = requires {
    { T::kind } -> std::convertible_to<BackwardKind>;
};

/// @brief The backward kind @p T publishes, or @c Unspecified if it publishes none.
template <typename T>
struct BackwardKindOf {
        static constexpr BackwardKind value = BackwardKind::Unspecified;
};

template <DeclaresBackwardKind T>
struct BackwardKindOf<T> {
        static constexpr BackwardKind value = T::kind;
};

/// @brief The backward kind @p T publishes, as a constant expression.
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

        /// @brief Which of the three backward forms this function implements. Defaults to
        ///        @c Unspecified, on which a bidirectional solve refuses to run.
        ///
        /// @return The backward form declared by this extension function.
        [[nodiscard]] virtual BackwardKind backward_kind() const {
            return BackwardKind::Unspecified;
        }

        /// @brief Extends a resource value along an arc in the backward direction.
        ///
        /// Defaults to calling @c extend(). Override for asymmetric resources.
        ///
        /// @p extender_value is the same direction-independent consumption the forward call gets;
        /// going backward, the node being left is the arc's destination.
        ///
        /// Forms: no bound -> @c Accumulate (inherit this); scalar with a bound -> @c Threshold
        /// (invert @c extend, clamp down by the node's upper bound); container with a bound ->
        /// @c Mirror (same formula, origin and destination swapped). @c Threshold must satisfy
        /// @code extend(x, arc) <= b   <==>   x <= extend_back(b, arc) @endcode
        ///
        /// @param resource        The current accumulated resource value.
        /// @param extender_value  The arc's contribution to the resource.
        /// @param extended_resource Pointer to the result; must not be null.
        virtual void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                                 ResourceType* extended_resource) {
            extend(resource, extender_value, extended_resource);
        }

        /// @brief A node's upper bound as the paired feasibility function states it, if it does.
        using CeilingSource = std::function<std::optional<ResourceType>(size_t node_id)>;

        /// @brief Receives the paired feasibility function's upper bounds, for clamping backward
        ///        extensions. Ignored by default; @c ThresholdForm uses it.
        ///
        /// @param ceiling_at The feasibility function's upper bound at a node, if it has one.
        virtual void adopt_ceilings(CeilingSource /*ceiling_at*/) {}

        /// @brief The value a forward extension arriving at @p node_id is clamped up to, if any.
        ///
        /// Read at setup to check a @c Threshold extension against its feasibility function's
        /// backward floor. @c ThresholdForm returns its lower bound. Must work for any node
        /// without preprocessing.
        ///
        /// @param node_id Index of the node.
        /// @return This node's forward floor, or @c std::nullopt for no clamp.
        [[nodiscard]] virtual auto floor_at(size_t /*node_id*/) const
            -> std::optional<ResourceType> {
            return std::nullopt;
        }

        /// @brief The value a backward extension arriving at @p node_id is clamped down to, if
        ///        any.
        ///
        /// Read at setup, to check it against that node's @c is_back_feasible. @c ThresholdForm
        /// returns the feasibility function's ceiling there, else its own upper bound.
        ///
        /// @param node_id Index of the node.
        /// @return This node's backward ceiling, or @c std::nullopt for no clamp.
        [[nodiscard]] virtual auto back_ceiling_at(size_t /*node_id*/) const
            -> std::optional<ResourceType> {
            return std::nullopt;
        }

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

/// @brief Thrown by a composed extension function that has no backward form.
///
/// A bidirectional solve checks for this once at setup and refuses the model.
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

        /// @brief Always @c Unspecified; a bidirectional solve does not read it.
        ///
        /// Setup reads each component's @c backward_kind() instead, and checks that this
        /// function has a backward form by probing @c extend_back once.
        ///
        /// @return @c BackwardKind::Unspecified unless overridden.
        [[nodiscard]] virtual BackwardKind backward_kind() const {
            return BackwardKind::Unspecified;
        }

        /// @brief Extends a composed resource along an arc in the backward direction.
        ///
        /// Not pure, so forward-only user compositions still compile; the default throws.
        /// @c CompositionExtensionFunction extends each component backward.
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
