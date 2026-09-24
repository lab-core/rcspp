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

/// @brief What a component cost function computes from its value, as far as the bidirectional
///        join needs to know.
///
/// The join adds a forward and a backward label's costs, which is exact only when each cost is a
/// sum over arcs. Setup refuses a model whose cost cannot be shown to be one.
enum class CostForm {
    Unspecified,  ///< not declared -- a bidirectional solve will refuse a cost that reads it
    Zero,         ///< always zero
    Value,        ///< the resource's value itself
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

        /// @brief What this function computes. Defaults to @c Unspecified, on which a
        ///        bidirectional solve refuses a cost that reads this component.
        ///
        /// @return The form declared by this cost function.
        [[nodiscard]] virtual CostForm cost_form() const { return CostForm::Unspecified; }

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
        /// @brief Optional node-specific preprocessing hook.
        ///
        /// Called by @c create() and @c reset(). Override to cache node-dependent data.
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

        /// @brief Whether the cost of a joined path is the forward half's cost plus the backward
        ///        half's.
        ///
        /// The bidirectional join and its pruning add the two halves' costs. That is exact only
        /// if every component this function reads has a cost that is zero, or that is its value
        /// under an accumulating extension; a backward @c Threshold value is a bound, not a
        /// consumption. Defaults to @c false, on which a bidirectional solve refuses.
        ///
        /// @param resource Any node's resource, to read the components' declarations from.
        /// @return @c true if the cost adds across a join.
        [[nodiscard]] virtual auto adds_across_join(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/) const -> bool {
            return false;
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
        /// @brief Optional node-specific preprocessing hook.
        ///
        /// Called by @c create() and @c reset(). Override to cache node-dependent data.
        ///
        /// @param node_id Index of the node being preprocessed.
        virtual void preprocess(size_t node_id) {}
};

}  // namespace rcspp
