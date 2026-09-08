// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <iostream>
#include <memory>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

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

/// @brief How a resource's backward extension relates to its forward one.
///
/// Declared per extension function via @c backward_kind(). A bidirectional solve refuses to start
/// on any component still reporting @c Unspecified. Forward-only solves never read it.
enum class BackwardKind {
    Unspecified,  ///< not declared -- a bidirectional solve will refuse to run
    Accumulate,   ///< no bound; extend_back == extend (e.g. cost)
    Threshold,    ///< stores a deadline/ceiling; extend_back inverts extend and clamps
    Mirror,       ///< stores a set seen on its own half; same formula, origin/destination swapped
};

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

        /// @brief Which of the three backward forms this function implements.
        ///
        /// Left @c Unspecified deliberately: a bidirectional solve refuses to run on a component
        /// whose author has not answered. See the decision procedure on @c extend_back below.
        ///
        /// @return The backward form declared by this extension function.
        [[nodiscard]] virtual BackwardKind backward_kind() const {
            return BackwardKind::Unspecified;
        }

        /// @brief Extends a resource value along an arc in the backward direction.
        ///
        /// Defaults to calling @c extend(). Override for asymmetric resources.
        ///
        /// **Contract.** @p extender_value is the arc's *direction-independent* consumption -- the
        /// same object the forward call receives. A backward extension must therefore derive any
        /// direction-dependent quantity itself: negate a magnitude, or substitute the node identity
        /// cached in @c preprocess(origin_id, destination_id). Going backward the node being *left*
        /// is the arc's destination, not its origin.
        ///
        /// **Which form do I need?**
        ///  - the resource has no feasibility bound   -> @c BackwardKind::Accumulate, inherit this
        ///  - a scalar value with a bound             -> @c BackwardKind::Threshold, invert
        ///                                               @c extend and clamp by this node's
        ///                                               *upper* bound (never up by its lower one)
        ///  - a container value with a bound          -> @c BackwardKind::Mirror, same formula with
        ///                                               origin and destination swapped
        ///
        /// For @c Threshold the correctness condition is a definition, not a property:
        /// @code extend(x, arc) <= b   <==>   x <= extend_back(b, arc) @endcode
        ///
        /// @param resource        The current accumulated resource value.
        /// @param extender_value  The arc's contribution to the resource.
        /// @param extended_resource Pointer to the result; must not be null.
        virtual void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                                 ResourceType* extended_resource) {
            extend(resource, extender_value, extended_resource);
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

        /// @brief Which of the three backward forms this function implements.
        ///
        /// Left @c Unspecified deliberately: a bidirectional solve refuses to run on a component
        /// whose author has not answered. See the decision procedure on @c extend_back below.
        ///
        /// @return The backward form declared by this extension function.
        [[nodiscard]] virtual BackwardKind backward_kind() const {
            return BackwardKind::Unspecified;
        }

        /// @brief Extends a resource value along an arc in the backward direction.
        ///
        /// Defaults to calling @c extend(). Override for asymmetric resources.
        ///
        /// **Contract.** @p extender_value is the arc's *direction-independent* consumption -- the
        /// same object the forward call receives. A backward extension must therefore derive any
        /// direction-dependent quantity itself: negate a magnitude, or substitute the node identity
        /// cached in @c preprocess(origin_id, destination_id). Going backward the node being *left*
        /// is the arc's destination, not its origin.
        ///
        /// **Which form do I need?**
        ///  - the resource has no feasibility bound   -> @c BackwardKind::Accumulate, inherit this
        ///  - a scalar value with a bound             -> @c BackwardKind::Threshold, invert
        ///                                               @c extend and clamp by this node's
        ///                                               *upper* bound (never up by its lower one)
        ///  - a container value with a bound          -> @c BackwardKind::Mirror, same formula with
        ///                                               origin and destination swapped
        ///
        /// For @c Threshold the correctness condition is a definition, not a property:
        /// @code extend(x, arc) <= b   <==>   x <= extend_back(b, arc) @endcode
        ///
        /// @param resource         The current accumulated composed resource.
        /// @param extender         The arc's extender carrying all component contributions.
        /// @param extended_resource Pointer to the result; must not be null.
        virtual void extend_back(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Extender<ResourceTypeComposition<ResourceTypes...>>& extender,
            Resource<ResourceTypeComposition<ResourceTypes...>>* extended_resource) {
            extend(resource, extender, extended_resource);
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
