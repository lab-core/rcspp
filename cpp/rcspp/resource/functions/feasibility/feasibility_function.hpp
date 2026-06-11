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

/// @brief Abstract base class defining the feasibility function for a resource type.
///
/// A feasibility function determines whether a label's accumulated resource
/// value satisfies the constraints of the problem (e.g., time-window or
/// capacity constraints) at a given node.
///
/// @tparam ResourceType The resource type satisfying @c ResourceTypeConcept.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class FeasibilityFunction {
    public:
        virtual ~FeasibilityFunction() = default;

        /// @brief Returns whether the given resource value is feasible in the forward direction.
        ///
        /// @param resource The accumulated resource value to test.
        /// @return @c true if the resource satisfies the forward feasibility constraint.
        [[nodiscard]] virtual auto is_feasible(const ResourceType& resource) -> bool = 0;

        /// @brief Returns whether the given resource value is feasible in the backward direction.
        ///
        /// Defaults to calling @c is_feasible(). Override for asymmetric constraints.
        ///
        /// @param resource The accumulated resource value to test.
        /// @return @c true if the resource satisfies the backward feasibility constraint.
        [[nodiscard]] virtual auto is_back_feasible(const ResourceType& resource) -> bool {
            return is_feasible(resource);
        }

        /// @brief Returns whether a forward and a backward label can be merged at this node.
        ///
        /// Used in bidirectional labeling to check join compatibility.
        ///
        /// @param resource      The forward label's resource value.
        /// @param back_resource The backward label's resource value.
        /// @return @c true if the two labels can be merged into a feasible path.
        /// @throws std::runtime_error If not overridden in a derived class.
        [[nodiscard]] virtual auto can_be_merged(const ResourceType& resource,
                                                 const ResourceType& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        /// @brief Returns whether the label can potentially reach a destination node.
        ///
        /// Used as a pruning test during label propagation. Defaults to @c true.
        ///
        /// @param resource            The current label holding the resource value.
        /// @param destination_node_id Index of the destination node to reach.
        /// @return @c true if reaching @p destination_node_id is still possible.
        virtual auto is_reachable(const Resource<ResourceType>& resource,
                                  size_t destination_node_id) -> bool {
            return true;
        }

        /// @brief Creates a polymorphic copy of this feasibility function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const -> std::unique_ptr<FeasibilityFunction> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new feasibility function instance ready for use at @p node_id.
        virtual auto create(const size_t node_id) -> std::unique_ptr<FeasibilityFunction> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
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

/// @brief Specialization of @c FeasibilityFunction for composed resource types.
///
/// When @c ResourceType is a @c ResourceTypeComposition, the feasibility function
/// receives the full @c Resource object so it can inspect all component values.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
// Specialization for ResourceTypeComposition: functions receive the full Resource object.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>> {
    public:
        virtual ~FeasibilityFunction() = default;

        /// @brief Returns whether the composed resource is feasible in the forward direction.
        ///
        /// @param resource The current label holding all component resource values.
        /// @return @c true if all relevant constraints are satisfied.
        [[nodiscard]] virtual auto is_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool = 0;

        /// @brief Returns whether the composed resource is feasible in the backward direction.
        ///
        /// Defaults to calling @c is_feasible(). Override for asymmetric constraints.
        ///
        /// @param resource The current label holding all component resource values.
        /// @return @c true if the backward feasibility constraint is satisfied.
        [[nodiscard]] virtual auto is_back_feasible(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource) -> bool {
            return is_feasible(resource);
        }

        /// @brief Returns whether a forward and a backward composed label can be merged.
        ///
        /// @param resource      The forward label.
        /// @param back_resource The backward label.
        /// @return @c true if merging the two labels yields a feasible path.
        /// @throws std::runtime_error If not overridden in a derived class.
        [[nodiscard]] virtual auto can_be_merged(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& resource,
            const Resource<ResourceTypeComposition<ResourceTypes...>>& back_resource) -> bool {
            throw std::runtime_error("FeasibilityFunction::can_be_merged not implemented");
        };

        /// @brief Returns whether the label can potentially reach a destination node.
        ///
        /// Defaults to @c true. Override to add reachability pruning.
        ///
        /// @param resource            The current label.
        /// @param destination_node_id Index of the destination node.
        /// @return @c true if reaching @p destination_node_id is still possible.
        virtual auto is_reachable(
            const Resource<ResourceTypeComposition<ResourceTypes...>>& /*resource*/,
            size_t /*destination_node_id*/) -> bool {
            return true;
        }

        /// @brief Creates a polymorphic copy of this feasibility function.
        ///
        /// @return A new heap-allocated copy wrapped in a unique_ptr.
        [[nodiscard]] virtual auto clone() const
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> = 0;

        /// @brief Clones this function and preprocesses it for a specific node.
        ///
        /// @param node_id Index of the node for which this function is instantiated.
        /// @return A new feasibility function instance ready for use at @p node_id.
        virtual auto create(const size_t node_id)
            -> std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>> {
            auto new_feasibility_function = clone();
            new_feasibility_function->preprocess(node_id);
            return new_feasibility_function;
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
