// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_prototype.hpp"
#include "rcspp/resource/composition/composition.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

/// @brief Specialization of `Resource` for a composed resource type.
///
/// Combines `ResourcePrototype` (which owns the dominance, feasibility, and cost functions)
/// with `Composition<Resource, ResourceTypes...>` so that each constituent resource type
/// can store its own sub-resources in the composition's component vectors.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class Resource<ResourceTypeComposition<ResourceTypes...>>
    : public ResourcePrototype<Resource<ResourceTypeComposition<ResourceTypes...>>,
                               ResourceTypeComposition<ResourceTypes...>>,
      public Composition<Resource, ResourceTypes...> {
        using Prototype = ResourcePrototype<Resource, ResourceTypeComposition<ResourceTypes...>>;

    public:
        /// @brief Default constructor.
        Resource() = default;

        /// @brief Full constructor taking owning-pointer functions and pre-built sub-resources.
        ///
        /// @param resource_components Tuple of per-type vectors of sub-resource owning pointers.
        /// @param dominance_function  Owning pointer to the dominance function.
        /// @param feasibility_function Owning pointer to the feasibility function.
        /// @param cost_function       Owning pointer to the cost function.
        /// @param node_id             Identifier of the node this resource is associated with.
        Resource(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id),
              Composition<Resource, ResourceTypes...>(std::move(resource_components)) {}

        /// @brief Constructor taking owning-pointer functions without pre-built sub-resources.
        ///
        /// @param dominance_function   Owning pointer to the dominance function.
        /// @param feasibility_function Owning pointer to the feasibility function.
        /// @param cost_function        Owning pointer to the cost function.
        /// @param node_id              Identifier of the node this resource is associated with.
        Resource(
            std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        /// @brief Constructor taking raw (non-owning) function pointers and pre-built
        /// sub-resources.
        ///
        /// The caller retains ownership of the function objects.
        ///
        /// @param resource_components  Tuple of per-type vectors of sub-resource owning pointers.
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Identifier of the node this resource is associated with.
        Resource(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            DominanceFunction<ResourceTypeComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceTypeComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Prototype(dominance_function, feasibility_function, cost_function, node_id),
              Composition<Resource, ResourceTypes...>(std::move(resource_components)) {}

        /// @brief Constructor taking raw (non-owning) function pointers without pre-built
        /// sub-resources.
        ///
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Identifier of the node this resource is associated with.
        Resource(
            DominanceFunction<ResourceTypeComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceTypeComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Prototype(dominance_function, feasibility_function, cost_function, node_id) {}

        /// @brief Deep-copy constructor.
        ///
        /// Copies both the prototype (function objects) and the composition (component resources).
        ///
        /// @param rhs_resource The resource to copy from.
        Resource(Resource const& rhs_resource)
            : Prototype(rhs_resource), Composition<Resource, ResourceTypes...>(rhs_resource) {}

        /// @brief Move constructor.
        ///
        /// @param rhs_resource The resource to move from.
        Resource(Resource&& rhs_resource) noexcept
            : Prototype(), Composition<Resource, ResourceTypes...>() {
            swap(*this, rhs_resource);
        }

        /// @brief Copy-and-swap assignment operator.
        ///
        /// @param rhs_resource The resource to assign from (passed by value).
        /// @return Reference to this resource after assignment.
        auto operator=(Resource rhs_resource) -> auto& {
            swap(*this, rhs_resource);
            return *this;
        }

        /// @brief Swaps the contents of two `Resource` objects.
        ///
        /// Swaps both the prototype and the composition parts.
        ///
        /// @param first  First resource.
        /// @param second Second resource.
        friend void swap(Resource& first, Resource& second) noexcept {
            using std::swap;
            swap(static_cast<Prototype&>(first), static_cast<Prototype&>(second));
            swap(static_cast<Composition<Resource, ResourceTypes...>&>(first),
                 static_cast<Composition<Resource, ResourceTypes...>&>(second));
        }

        /// @brief Creates a new resource for a given node, ignoring the value argument.
        ///
        /// Composition resources carry no scalar value of their own; this override simply
        /// delegates to `create(node_id)`.
        ///
        /// @param resource_value Unused composition value (composition has no scalar value).
        /// @param node_id        Identifier of the target node.
        /// @return Owning pointer to the newly created resource.
        [[nodiscard]] auto create(
            const ResourceTypeComposition<ResourceTypes...>& /*resource_value*/,
            const size_t node_id) const -> std::unique_ptr<Resource> {
            return create(node_id);
        }

        /// @brief Creates a new resource for a given node, cloning all sub-resource prototypes.
        ///
        /// Each sub-resource in the composition calls its own `create(node_id)`, and the
        /// prototype's dominance, feasibility, and cost functions are cloned as well.
        ///
        /// @param node_id Identifier of the target node.
        /// @return Owning pointer to the newly created resource.
        [[nodiscard]] auto create(const size_t node_id) const -> auto {
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                new_resource_components;
            this->apply(new_resource_components,
                        [&](const auto& sing_res_vec, auto& sing_new_res_vec) {
                            std::transform(
                                sing_res_vec.begin(),
                                sing_res_vec.end(),
                                std::back_inserter(sing_new_res_vec),
                                [node_id](const auto& res) { return res->create(node_id); });
                        });

            return std::make_unique<Resource>(std::move(new_resource_components),
                                              this->dominance_function_->create(node_id),
                                              this->feasibility_function_->create(node_id),
                                              this->cost_function_->create(node_id),
                                              node_id);
        }

        /// @brief Copies this resource, sharing (not cloning) the function objects.
        ///
        /// Sub-resources are deep-copied but all function pointers remain shared with the
        /// original.  Use `create(node_id)` when independent function objects are required.
        ///
        /// @return Owning pointer to the copied resource.
        [[nodiscard]] auto copy() const -> std::unique_ptr<Resource> {
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                new_resource_components;
            this->apply(new_resource_components,
                        [&](const auto& sing_res_vec, auto& sing_new_res_vec) {
                            std::transform(sing_res_vec.begin(),
                                           sing_res_vec.end(),
                                           std::back_inserter(sing_new_res_vec),
                                           [](const auto& res) { return res->copy(); });
                        });

            return std::make_unique<Resource>(std::move(new_resource_components),
                                              this->dominance_function_,
                                              this->feasibility_function_,
                                              this->cost_function_,
                                              this->node_id_);
        }

        /// @brief Returns a full clone of this resource (prototype + composition).
        ///
        /// Delegates to `Prototype::clone()`, which deep-copies function objects and
        /// sub-resources.
        ///
        /// @return Owning pointer to the cloned resource.
        [[nodiscard]] auto clone() const -> auto { return Prototype::clone(); }

        /// @brief Resets all sub-resources and the prototype to the given node.
        ///
        /// @param node_id Identifier of the target node.
        void reset(size_t node_id) {
            Prototype::reset(node_id);
            this->for_each_component([node_id](auto&& res) { res->reset(node_id); });
        }

        /// @brief Resets all sub-resources by copying state from another composition.
        ///
        /// @param other_composition The composition to copy state from.
        void reset(const Resource& other_composition) {
            Prototype::reset(other_composition);
            this->for_each_component(other_composition, [](auto&& res_comp, auto&& other_res_comp) {
                res_comp.reset(*other_res_comp);
            });
        }

        /// @brief Checks whether this resource is dominated by @p rhs_resource.
        ///
        /// Passes the full composition objects to the dominance function.
        ///
        /// @param rhs_resource The resource to compare against.
        /// @return `true` if this resource is dominated by (i.e., `<=`) @p rhs_resource.
        auto operator<=(const Resource& rhs_resource) const -> bool {
            return this->dominance_function_->check_dominance(*this, rhs_resource);
        }

        /// @brief Returns the total cost of this composed resource.
        ///
        /// @return The cost value computed by the cost function.
        [[nodiscard]] auto get_cost() const -> double {
            return this->cost_function_->get_cost(*this);
        }

        /// @brief Returns `true` if this resource satisfies all forward-direction constraints.
        ///
        /// @return Result of the feasibility function's `is_feasible` check.
        [[nodiscard]] auto is_feasible() const -> bool {
            return this->feasibility_function_->is_feasible(*this);
        }

        /// @brief Returns `true` if this resource satisfies all backward-direction constraints.
        ///
        /// @return Result of the feasibility function's `is_back_feasible` check.
        [[nodiscard]] auto is_back_feasible() const -> bool {
            return this->feasibility_function_->is_back_feasible(*this);
        }

        /// @brief Returns `true` if this (forward) resource can be merged with @p back_resource.
        ///
        /// @param back_resource The backward resource to merge with.
        /// @return Result of the feasibility function's `can_be_merged` check.
        [[nodiscard]] auto can_be_merged(const Resource& back_resource) const -> bool {
            return this->feasibility_function_->can_be_merged(*this, back_resource);
        }

        /// @brief Returns `true` if this resource can reach the given destination node.
        ///
        /// @param destination_node_id Identifier of the destination node.
        /// @return Result of the feasibility function's `is_reachable` check.
        [[nodiscard]] auto is_reachable(size_t destination_node_id) const -> bool {
            return this->feasibility_function_->is_reachable(*this, destination_node_id);
        }
};

}  // namespace rcspp
