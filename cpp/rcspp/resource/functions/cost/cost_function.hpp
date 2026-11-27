// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <optional>
#include <utility>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource;

/// @brief What a cost function computes, as far as a backward search and the join need to know.
///
/// A backward label's cost, and the join of a forward and a backward label's costs, are right only
/// for an additive cost: one that sums over arcs. The kind lets @c BackwardExtensionCheck check
/// that without evaluating the function.
enum class CostKind {
    Unspecified,  ///< not declared: a backward or bidirectional solve refuses a cost that reads it
    Zero,         ///< always zero, so additive
    Value,        ///< the resource's value, additive only under an accumulation that is a sum;
                  ///< exactly the value: the cost order inferred from the dominance relies on it
};

/// @brief Abstract base class defining the cost function for a resource type.
///
/// A cost function maps a label's accumulated resource value to a scalar cost.
/// The cost is used to rank or compare labels during the shortest-path search.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class CostFunction {
    public:
        virtual ~CostFunction() = default;

        /// @brief Returns the cost associated with the given resource value.
        ///
        /// @param resource The accumulated resource value of a label.
        /// @return The scalar cost for this resource.
        [[nodiscard]] virtual auto get_cost(const ResourceType& resource) const -> double = 0;

        /// @brief What this function computes, for the cost check of @c BackwardExtensionCheck.
        ///
        /// Defaults to @c Unspecified, on which a backward or bidirectional solve refuses a cost
        /// that reads this component.
        ///
        /// @return The kind declared by this cost function.
        [[nodiscard]] virtual CostKind cost_kind() const { return CostKind::Unspecified; }

        /// @brief Creates a polymorphic copy of this cost function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<CostFunction> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new cost function instance ready for use at @p node_id.
        auto create(const size_t node_id) -> std::unique_ptr<CostFunction> {
            auto new_cost_function = clone();
            new_cost_function->preprocess(node_id);
            return new_cost_function;
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

/// @brief Specialization of @c CostFunction for composed resource types.
///
/// When @c ResourceType is a @c ResourceTypeComposition, the cost function
/// receives the full @c Resource object so it can read all component values.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
// Specialization for ResourceTypeComposition: functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class CostFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~CostFunction() = default;

        /// @brief Returns the cost associated with the given composed resource.
        ///
        /// @param resource The current label holding all component resource values.
        /// @return The scalar cost derived from the composed resource.
        [[nodiscard]] virtual auto get_cost(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) const
            -> double = 0;

        /// @brief Whether this function's cost is additive: the cost of a path is the sum of
        ///        the costs of its two halves, wherever it is split.
        ///
        /// A backward label's cost is its suffix's, and the bidirectional join and its pruning add
        /// the two halves' costs, so a backward or bidirectional solve refuses a cost that is not
        /// additive. It is if every component this function reads has an additive cost
        /// (@c Resource::is_cost_additive). Defaults to @c false, on which those solves refuse.
        ///
        /// @param resource Any node's resource, to read the components' declarations from.
        /// @return @c true if the cost is additive.
        [[nodiscard]] virtual auto is_additive(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/) const -> bool {
            return false;
        }

        /// @brief The cost order a component-wise dominance gives this cost: an @c eps such that,
        ///        when every component of @c a dominates the same component of @c b, this
        ///        function's cost of @c a is at most its cost of @c b + @c eps; empty when the
        ///        function cannot tell, the default.
        ///
        /// A dominance function that declares @c DominanceFunction::requires_component_dominance
        /// and no tolerance of its own (@c CompositionDominanceFunction) gets this one, which lets
        /// @c CostOrderedLabelList skip the pairs the costs rule out. Each component tells its own
        /// (@c Resource::component_cost_order_tolerance). A function that adds several components'
        /// costs should declare nothing when more than one may be nonzero: the rounding of the sum
        /// may break the promise by more than the one ulp the container allows.
        ///
        /// @param resource Any node's resource, to read the components' declarations from.
        /// @param backward Whether the components backward-dominate (@c Resource::back_dominates)
        ///                 rather than dominate.
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] virtual auto component_wise_cost_order_tolerance(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            bool /*backward*/) const -> std::optional<double> {
            return std::nullopt;
        }

        /// @brief Creates a polymorphic copy of this cost function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new cost function instance ready for use at @p node_id.
        auto create(const size_t node_id)
            -> std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_cost_function = clone();
            new_cost_function->preprocess(node_id);
            return new_cost_function;
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
