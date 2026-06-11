// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/resource_prototype.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

/// @brief Concrete resource type used during label extension in the RCSPP algorithm.
///
/// `Resource` is the primary building block of a label: it holds a typed resource value
/// together with the dominance, feasibility, and cost functions required to evaluate that
/// label at a given graph node.
///
/// This class is the leaf of the `ResourcePrototype` CRTP hierarchy and adds the
/// domain-level query operations (`operator<=`, `is_lower`, `get_cost`, `is_feasible`,
/// etc.) that are called by the solver's inner loop.
///
/// @tparam ResourceType The value type stored inside this resource; must satisfy
///                      `ResourceTypeConcept`.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource : public ResourcePrototype<Resource<ResourceType>, ResourceType> {
        using Prototype = ResourcePrototype<Resource, ResourceType>;

    public:
        /// @brief Default constructor.
        Resource() = default;

        /// @brief Constructs a resource with a copied value and exclusively-owned function objects.
        ///
        /// @param resource_value       Initial resource value (copied).
        /// @param dominance_function   Owned dominance function.
        /// @param feasibility_function Owned feasibility function.
        /// @param cost_function        Owned cost function.
        /// @param node_id              Associated graph node (default 0).
        Resource(const ResourceType& resource_value,
                 std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : Prototype(resource_value, std::move(dominance_function),
                        std::move(feasibility_function), std::move(cost_function), node_id) {}

        /// @brief Constructs a default-value resource with exclusively-owned function objects.
        ///
        /// @param dominance_function   Owned dominance function.
        /// @param feasibility_function Owned feasibility function.
        /// @param cost_function        Owned cost function.
        /// @param node_id              Associated graph node (default 0).
        Resource(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        /// @brief Constructs a resource with a copied value and borrowed (non-owning) function
        ///        objects.
        ///
        /// @param resource_value       Initial resource value (copied).
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Associated graph node (default 0).
        Resource(const ResourceType& resource_value,
                 DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : Prototype(resource_value, std::move(dominance_function),
                        std::move(feasibility_function), std::move(cost_function), node_id) {}

        /// @brief Constructs a default-value resource with borrowed (non-owning) function objects.
        ///
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Associated graph node (default 0).
        Resource(DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        /// @brief Copy constructor.
        ///
        /// @param rhs_resource Resource to copy.
        Resource(Resource const& rhs_resource) : Prototype(rhs_resource) {}

        /// @brief Move constructor.
        ///
        /// @param rhs_resource Resource to move from.
        Resource(Resource&& rhs_resource) noexcept : Prototype(std::move(rhs_resource)) {}

        /// @brief Swaps two `Resource` objects without throwing.
        ///
        /// @param first  First resource.
        /// @param second Second resource.
        static void swap(Resource& first, Resource& second) noexcept {
            ResourcePrototype<Resource, ResourceType>::swap(first, second);
        }

        // Check dominance — passes value_ for simple types, full Resource for composition types
        /// @brief Dominance check: returns `true` if this resource dominates `rhs_resource`.
        ///
        /// A resource `a` dominates `b` (`a <= b`) when `a` is at least as good as `b` on all
        /// dimensions according to the configured `DominanceFunction`.
        ///
        /// @param rhs_resource The resource to compare against.
        /// @return `true` if this resource dominates `rhs_resource`.
        auto operator<=(const Resource& rhs_resource) const -> bool {
            return this->dominance_function_->check_dominance(this->value_, rhs_resource.value_);
        }

        // Check distance from the resource to another
        /// @brief Fast dominance check with a relaxation delta.
        ///
        /// Uses the dominance function's `fast_check_dominance` method, which may apply an
        /// additive tolerance `delta` to speed up dominance screening.
        ///
        /// @param rhs_resource The resource to compare against.
        /// @param delta        Relaxation tolerance (default 0).
        /// @return `true` if this resource is considered lower than (dominated by) `rhs_resource`
        ///         within the given tolerance.
        [[nodiscard]] auto is_lower(const Resource& rhs_resource, double delta = 0) const -> bool {
            return this->dominance_function_->fast_check_dominance(this->value_,
                                                                   rhs_resource.value_,
                                                                   delta);
        }

        // Return resource cost
        /// @brief Returns the scalar cost associated with this resource's current value.
        ///
        /// @return Cost as computed by the configured `CostFunction`.
        [[nodiscard]] auto get_cost() const -> double {
            return this->cost_function_->get_cost(this->value_);
        }

        // Return true if the resource is feasible
        /// @brief Returns `true` if this resource satisfies all forward-direction feasibility
        ///        constraints.
        ///
        /// @return `true` when the resource is feasible in the forward direction.
        [[nodiscard]] auto is_feasible() const -> bool {
            return this->feasibility_function_->is_feasible(this->value_);
        }

        /// @brief Returns `true` if this resource satisfies all backward-direction feasibility
        ///        constraints.
        ///
        /// @return `true` when the resource is feasible in the backward direction.
        [[nodiscard]] auto is_back_feasible() const -> bool {
            return this->feasibility_function_->is_back_feasible(this->value_);
        }

        /// @brief Returns `true` if this (forward) resource can be merged with a backward label.
        ///
        /// Used in bidirectional labelling to determine whether a forward and a backward label
        /// can be joined into a complete path.
        ///
        /// @param back_resource The backward resource to attempt merging with.
        /// @return `true` when the two labels are compatible for merging.
        [[nodiscard]] auto can_be_merged(const Resource& back_resource) const -> bool {
            return this->feasibility_function_->can_be_merged(this->value_, back_resource.value_);
        }

        /// @brief Returns `true` if the destination node is reachable from this resource's state.
        ///
        /// Delegates to the feasibility function's reachability check, which may use ng-route
        /// or other neighbourhood information.
        ///
        /// @param destination_node_id Identifier of the node whose reachability is queried.
        /// @return `true` when the destination node can still be reached.
        [[nodiscard]] auto is_reachable(size_t destination_node_id) const -> bool {
            return this->feasibility_function_->is_reachable(*this, destination_node_id);
        }
};
}  // namespace rcspp
