// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/utils/utils.hpp"

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
        /// When it does, @p lhs_resource is at least as good in every dimension, so the label
        /// holding @p rhs_resource can be discarded in favour of the one holding @p lhs_resource.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @return @c true if @p lhs_resource dominates @p rhs_resource.
        [[nodiscard]] virtual auto check_dominance(const ResourceType& lhs_resource,
                                                   const ResourceType& rhs_resource) -> bool = 0;

        // clang-format off
        /// @brief A fast, possibly partial, dominance check, shifted by @p delta.
        ///
        /// For a scalar it is the dominance test itself, with @p delta in @p lhs_resource's
        /// favour. For a container it is a cheaper necessary condition, a size comparison, so it
        /// can pass where @c check_dominance() fails. Useful as a quick filter; @c LabelBuckets
        /// also uses it, through @c Resource::is_lower, to place a value on its bucket axis.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @param delta        An offset, not only a tolerance: a positive one makes the check
        ///                     easier to pass, a negative one harder (@c LabelBuckets passes
        ///                     @c -range).
        /// @return @c true if @p lhs_resource passes the check against @p rhs_resource, shifted
        ///         by @p delta.
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
        void set_back_dominance_reversed(bool reversed) { back_dominance_reversed_ = reversed; }

        /// @brief Whether backward dominance reverses the forward comparison; see
        ///        @c set_back_dominance_reversed.
        ///
        /// @return @c true when the backward comparison is the forward one, swapped.
        [[nodiscard]] bool is_back_dominance_reversed() const { return back_dominance_reversed_; }

        /// @brief Whether @p lhs_resource dominates @p rhs_resource as backward labels at the same
        ///        node: the backward search's dominance test.
        ///
        /// The forward test, with the arguments swapped when the backward value is a limit.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @return @c true if @p lhs_resource dominates @p rhs_resource going backward.
        [[nodiscard]] virtual auto check_back_dominance(const ResourceType& lhs_resource,
                                                        const ResourceType& rhs_resource) -> bool {
            return back_dominance_reversed_ ? check_dominance(rhs_resource, lhs_resource)
                                            : check_dominance(lhs_resource, rhs_resource);
        }

        /// @brief @c fast_check_dominance going backward: its arguments are swapped when the
        ///        backward value is a limit, as in @c check_back_dominance. For bucketed backward
        ///        label containers.
        ///
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @param delta        An offset, as in @c fast_check_dominance: a positive one makes the
        ///                     check easier to pass, a negative one harder.
        /// @return @c true if @p lhs_resource passes the backward check against @p rhs_resource,
        ///         shifted by @p delta.
        virtual auto fast_check_back_dominance(const ResourceType& lhs_resource,
                                               const ResourceType& rhs_resource, double delta)
            -> bool {
            return back_dominance_reversed_
                       ? fast_check_dominance(rhs_resource, lhs_resource, delta)
                       : fast_check_dominance(lhs_resource, rhs_resource, delta);
        }

        /// @brief A promise on a scalar value: an @c eps >= 0 such that
        ///        `check_dominance(a, b)` implies `a.get_value() <= b.get_value() + eps`; empty
        ///        when the function promises nothing, the default.
        ///
        /// @c CompositionDominanceFunction reads it, with the component's cost function, to infer
        /// the composition's cost order (@c DominanceFunction::cost_order_tolerance): a component
        /// whose cost is its value (@c CostKind::Value) passes it on. @c ValueDominanceFunction
        /// declares it. A function derived from one that declares it and tests otherwise must
        /// declare its own; a wrong promise makes a search on @c CostOrderedLabelList slower,
        /// never wrong (and @c RCSPP_VERIFY_COST_ORDER catches it).
        ///
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] virtual auto value_order_tolerance() const -> std::optional<double> {
            return std::nullopt;
        }

        /// @brief The same promise for @c check_back_dominance.
        ///
        /// By default that of @ref value_order_tolerance when the backward test is the forward
        /// one, and none when it swaps the arguments (a limit, where the larger value is better).
        ///
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] virtual auto back_value_order_tolerance() const -> std::optional<double> {
            return back_dominance_reversed_ ? std::nullopt : value_order_tolerance();
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
        /// @brief Optional per-node preprocessing: precomputes what this function needs at
        ///        @p node_id, such as the node's time window, so that it is not looked up on
        ///        every call.
        ///
        /// Called on each clone by @c create() and @c reset(), whenever the function is bound to
        /// a node. Unrelated to the graph preprocessing that the @c preprocess argument of
        /// @c ResourceGraph::solve controls.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}

        /// @brief Whether backward dominance swaps the forward comparison's arguments.
        bool back_dominance_reversed_ = false;
};

/// @brief Thrown by a composed function whose backward member (@c check_back_dominance here) is not
///        overridden.
///
/// @c BackwardExtensionCheck calls it once before any search, and turns this exception into a
/// refusal that names the missing override.
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
        /// @param lhs_resource The candidate dominating resource.
        /// @param rhs_resource The resource being tested for dominance.
        /// @return @c true if @p lhs_resource dominates @p rhs_resource.
        [[nodiscard]] virtual auto check_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& lhs_resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& rhs_resource) -> bool = 0;

        /// @brief Whether a composed backward label dominates another.
        ///
        /// The default throws @c NoBackwardDominance, so a forward-only composition still compiles
        /// and the model checks refuse it; @c CompositionDominanceFunction compares component
        /// by component.
        ///
        /// @param lhs_resource The label being tested for dominance.
        /// @param rhs_resource The reference label.
        /// @return @c true if @p lhs_resource backward-dominates @p rhs_resource.
        /// @throws NoBackwardDominance unless overridden.
        [[nodiscard]] virtual auto check_back_dominance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*lhs_resource*/,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*rhs_resource*/) -> bool {
            throw NoBackwardDominance("the composition dominance function " +
                                      short_type_name(*this) +
                                      " cannot be read backward: override check_back_dominance, as "
                                      "CompositionDominanceFunction does");
        }

        /// @brief A promise on the cost: an @c eps >= 0 such that @c check_dominance(a, b) implies
        ///        `a.get_cost() <= b.get_cost() + eps`; empty when the function promises nothing,
        ///        the default.
        ///
        /// A container that keeps its labels in cost order (@c CostOrderedLabelList) then skips
        /// the pairs the costs rule out: a label is never dominated by one costing more than its
        /// cost + @c eps, nor dominates one costing less than its cost − @c eps. A dominance
        /// function that rejects a pair as soon as the first label costs more, as one comparing
        /// the costs first does, declares the tolerance of that comparison. The promise is read
        /// in exact arithmetic: a function that compares a rounded `a - eps <= b` may accept a
        /// pair the exact inequality rules out by an ulp; the container widens its bounds by one
        /// ulp, which covers such a rounding of one operation.
        ///
        /// A wrong promise is slow, never wrong, for an exact search: the container skips only
        /// pairs that could have removed a label, and keeping a label dominance would have
        /// removed loses no path. Under a cap on the labels a node keeps or extends, the labels
        /// kept too may change which others the cap keeps. Built with @c RCSPP_VERIFY_COST_ORDER
        /// (@c kVerifyCostOrder), the container tests the pairs it skips too, and throws on one
        /// that is a dominance.
        ///
        /// When it is empty and the function declares @ref requires_component_dominance, as
        /// @c CompositionDominanceFunction does, the promise is inferred from the components
        /// instead (@c CostFunction::component_wise_cost_order_tolerance).
        ///
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] virtual auto cost_order_tolerance() const -> std::optional<double> {
            return std::nullopt;
        }

        /// @brief The same promise for @c check_back_dominance: `check_back_dominance(a, b)`
        ///        implies `a.get_cost() <= b.get_cost() + eps`, for backward labels. Empty by
        ///        default, whatever @ref cost_order_tolerance says.
        ///
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] virtual auto back_cost_order_tolerance() const -> std::optional<double> {
            return std::nullopt;
        }

        /// @brief Whether a dominance needs every component to dominate: `check_dominance(a, b)`
        ///        only when each component of @c a dominates the same component of @c b
        ///        (`a_i <= b_i`), and `check_back_dominance(a, b)` only when each
        ///        backward-dominates it (`a_i.back_dominates(b_i)`). False by default.
        ///
        /// When it is true and the function declares no @ref cost_order_tolerance, the
        /// composition infers one from the components and the cost function
        /// (@c CostFunction::component_wise_cost_order_tolerance): a test as strict as the
        /// components', or stricter, keeps their promise. @c CompositionDominanceFunction
        /// declares it; a function derived from it that relaxes a component must not.
        ///
        /// @return @c true if a dominance needs every component's.
        [[nodiscard]] virtual auto requires_component_dominance() const -> bool { return false; }

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
        /// @brief Optional per-node preprocessing: precomputes what this function needs at
        ///        @p node_id, such as the node's time window, so that it is not looked up on
        ///        every call.
        ///
        /// Called on each clone by @c create() and @c reset(), whenever the function is bound to
        /// a node. Unrelated to the graph preprocessing that the @c preprocess argument of
        /// @c ResourceGraph::solve controls.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
