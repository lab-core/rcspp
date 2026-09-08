// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
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
        /// Derived -- never declared by hand. @c ResourceGraph::add_resource sets it from the
        /// extension function's @c backward_kind(), so the two can never disagree.
        ///
        /// @param reversed @c true when the backward comparison is the forward one, swapped.
        void set_backward_reversed(bool reversed) { backward_reversed_ = reversed; }

        /// @brief Returns whether backward dominance reverses the forward comparison.
        ///
        /// @return @c true when the backward comparison is the forward one, swapped.
        [[nodiscard]] bool is_backward_reversed() const { return backward_reversed_; }

        /// @brief Returns whether @p lhs_resource dominates @p rhs_resource going backward.
        ///
        /// Reversing dominance is exactly an argument swap, tolerance included, so no concrete
        /// dominance function needs to implement anything.
        ///
        /// @param lhs_resource The resource value of the label being tested for dominance.
        /// @param rhs_resource The resource value of the reference label.
        /// @return @c true if @p lhs_resource is dominated by @p rhs_resource going backward.
        [[nodiscard]] virtual auto check_back_dominance(const ResourceType& lhs_resource,
                                                        const ResourceType& rhs_resource) -> bool {
            return backward_reversed_ ? check_dominance(rhs_resource, lhs_resource)
                                      : check_dominance(lhs_resource, rhs_resource);
        }

        /// @brief Fast (possibly partial) backward dominance check with a tolerance.
        ///
        /// @param lhs_resource The resource value of the label being tested.
        /// @param rhs_resource The resource value of the reference label.
        /// @param delta        Tolerance used for approximate dominance comparisons.
        /// @return @c true if @p lhs_resource is (partially) backward-dominated by
        ///         @p rhs_resource.
        virtual auto fast_check_back_dominance(const ResourceType& lhs_resource,
                                               const ResourceType& rhs_resource, double delta)
            -> bool {
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

        /// @brief Whether backward dominance reverses the forward comparison.
        bool backward_reversed_ = false;
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

        /// @brief Returns whether @p lhs_resource dominates @p rhs_resource going backward.
        ///
        /// Implemented by fanning out over the components, each of which applies its own
        /// (possibly reversed) backward comparison.
        ///
        /// @param lhs_resource The label being tested for dominance.
        /// @param rhs_resource The reference label.
        /// @return @c true if @p lhs_resource is backward-dominated by @p rhs_resource.
        [[nodiscard]] virtual auto check_back_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_resource) -> bool = 0;

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
