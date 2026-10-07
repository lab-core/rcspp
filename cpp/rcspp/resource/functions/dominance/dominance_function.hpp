// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

/// @brief Abstract base class defining the dominance function for a resource type.
///
/// A dominance function determines whether one label's resource value dominates
/// another, allowing the dominated label to be safely pruned during the RCSPP
/// labeling algorithm.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class DominanceFunction {
    public:
        virtual ~DominanceFunction() = default;

        /// @brief Returns whether @p lhs_resource dominates @p rhs_resource.
        ///
        /// A label with @p lhs_resource can be discarded in favour of @p rhs_resource
        /// if @p lhs_resource dominates it (i.e., @p rhs_resource is at least as good
        /// in every dimension).
        ///
        /// @param lhs_resource The resource value of the label being tested for dominance.
        /// @param rhs_resource The resource value of the reference label.
        /// @return @c true if @p lhs_resource is dominated by @p rhs_resource.
        [[nodiscard]] virtual auto check_dominance(const ResourceType& lhs_resource,
                                                   const ResourceType& rhs_resource) -> bool = 0;

        // clang-format off
        /// @brief Performs a fast (possibly partial) dominance check with a tolerance.
        ///
        /// Useful as a quick filter before calling the full @c check_dominance(), or
        /// when the dominance data structure supports approximate queries.
        ///
        /// @param lhs_resource The resource value of the label being tested.
        /// @param rhs_resource The resource value of the reference label.
        /// @param delta        Tolerance used for approximate dominance comparisons.
        /// @return @c true if @p lhs_resource is (partially) dominated by @p rhs_resource.
        // Use to check (partial) dominance quickly. Useful for more complex data structure
        virtual auto fast_check_dominance(const ResourceType& lhs_resource,
                                          const ResourceType& rhs_resource, double delta)
            -> bool = 0;
        // clang-format on

        /// @brief Sets whether backward dominance reverses the forward comparison.
        ///
        /// A threshold's backward value is a limit, so a larger one is better (a later deadline,
        /// more room left). Set by @c ResourceGraph::add_resource from the extension's backward
        /// kind, through @c reverses_back_dominance; not meant to be set by hand.
        ///
        /// @param reversed @c true when the backward comparison is the forward one, swapped.
        void set_backward_reversed(bool reversed) { backward_reversed_ = reversed; }

        /// @brief Whether backward dominance reverses the forward comparison; see
        ///        @c set_backward_reversed.
        ///
        /// @return @c true when the backward comparison is the forward one, swapped.
        [[nodiscard]] bool is_backward_reversed() const { return backward_reversed_; }

        /// @brief Whether @p lhs_resource dominates @p rhs_resource as backward labels at the same
        ///        node: the backward search's dominance test.
        ///
        /// The forward test, with the arguments swapped when the backward value is a limit.
        ///
        /// @param lhs_resource The resource value of the label being tested for dominance.
        /// @param rhs_resource The resource value of the reference label.
        /// @return @c true if @p lhs_resource dominates @p rhs_resource going backward.
        [[nodiscard]] virtual auto check_back_dominance(const ResourceType& lhs_resource,
                                                        const ResourceType& rhs_resource) -> bool {
            return backward_reversed_ ? check_dominance(rhs_resource, lhs_resource)
                                      : check_dominance(lhs_resource, rhs_resource);
        }

        /// @brief The relaxed backward dominance test, within @p delta: the backward counterpart of
        ///        @c fast_check_dominance, for bucketed backward label containers.
        ///
        /// @param lhs_resource The resource value of the label being tested.
        /// @param rhs_resource The resource value of the reference label.
        /// @param delta        Tolerance used for approximate dominance comparisons.
        /// @return @c true if @p lhs_resource is (partially) backward-dominated by
        ///         @p rhs_resource.
        virtual auto fast_check_back_dominance(const ResourceType& lhs_resource,
                                               const ResourceType& rhs_resource,
                                               double delta) -> bool {
            return backward_reversed_ ? fast_check_dominance(rhs_resource, lhs_resource, delta)
                                      : fast_check_dominance(lhs_resource, rhs_resource, delta);
        }

        /// @brief Creates a polymorphic copy of this dominance function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<DominanceFunction> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new dominance function instance ready for use at @p node_id.
        auto create(const size_t node_id) -> std::unique_ptr<DominanceFunction> {
            auto new_dominance_function = clone();
            new_dominance_function->preprocess(node_id);
            return new_dominance_function;
        }

        /// @brief Resets this function's state for a new node.
        ///
        /// @param node_id Index of the node to reset for.
        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        /// @brief Optional node-specific preprocessing hook.
        ///
        /// Called by @c create() and @c reset(). Override to cache node-dependent data.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}

        /// @brief Whether backward dominance swaps the forward comparison's arguments.
        bool backward_reversed_ = false;
};

/// @brief Thrown by a composed function whose backward member (@c check_back_dominance here) is not
///        overridden.
///
/// The bidirectional setup calls it once before searching, and turns this exception into a refusal
/// that names the missing override.
class NoBackwardDominance : public std::logic_error {
    public:
        using std::logic_error::logic_error;
};

/// @brief Specialization of @c DominanceFunction for composed resource types.
///
/// When @c ResourceType is a @c ResourceTypeComposition, the dominance function
/// receives full @c Resource objects so it can compare all component values.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
// Specialization for ResourceTypeComposition: functions receive the full Resource object
// since the composition tag carries no values of its own.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class DominanceFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~DominanceFunction() = default;

        /// @brief Returns whether @p lhs_resource dominates @p rhs_resource.
        ///
        /// @param lhs_resource The label being tested for dominance.
        /// @param rhs_resource The reference label.
        /// @return @c true if @p lhs_resource is dominated by @p rhs_resource.
        [[nodiscard]] virtual auto check_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_resource) -> bool = 0;

        /// @brief Whether a composed backward label dominates another.
        ///
        /// The default throws @c NoBackwardDominance, so a forward-only composition still compiles
        /// and a bidirectional setup refuses it; @c CompositionDominanceFunction compares component
        /// by component.
        ///
        /// @param lhs_resource The label being tested for dominance.
        /// @param rhs_resource The reference label.
        /// @return @c true if @p lhs_resource backward-dominates @p rhs_resource.
        /// @throws NoBackwardDominance unless overridden.
        [[nodiscard]] virtual auto check_back_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*lhs_resource*/,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*rhs_resource*/) -> bool {
            throw NoBackwardDominance(
                "this composition dominance function has no backward form: override "
                "check_back_dominance, as CompositionDominanceFunction does");
        }

        /// @brief Creates a polymorphic copy of this dominance function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new dominance function instance ready for use at @p node_id.
        auto create(const size_t node_id)
            -> std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_dominance_function = clone();
            new_dominance_function->preprocess(node_id);
            return new_dominance_function;
        }

        /// @brief Resets this function's state for a new node.
        ///
        /// @param node_id Index of the node to reset for.
        virtual void reset(const size_t node_id) { preprocess(node_id); }

    protected:
        /// @brief Optional node-specific preprocessing hook.
        ///
        /// Called by @c create() and @c reset(). Override to cache node-dependent data.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
