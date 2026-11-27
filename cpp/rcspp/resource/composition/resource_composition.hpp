// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_prototype.hpp"
#include "rcspp/resource/composition/composition.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/utils/utils.hpp"

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
                            sing_new_res_vec.reserve(sing_res_vec.size());
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
            // Reserved: a new label is a copy of its node's resource, and growing each vector one
            // component at a time costs several allocations per label.
            this->apply(new_resource_components,
                        [&](const auto& sing_res_vec, auto& sing_new_res_vec) {
                            sing_new_res_vec.reserve(sing_res_vec.size());
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
            invalidate_cost();
            Prototype::reset(node_id);
            this->for_each_component([node_id](auto&& res) { res->reset(node_id); });
        }

        /// @brief Resets all sub-resources by copying state from another composition.
        ///
        /// @param other_composition The composition to copy state from.
        void reset(const Resource& other_composition) {
            invalidate_cost();
            Prototype::reset(other_composition);
            this->for_each_component(other_composition, [](auto&& res_comp, auto&& other_res_comp) {
                res_comp.reset(*other_res_comp);
            });
        }

        /// @brief Checks whether this resource dominates @p rhs_resource.
        ///
        /// Passes the full composition objects to the dominance function.
        ///
        /// @param rhs_resource The resource to compare against.
        /// @return `true` if this resource dominates @p rhs_resource.
        auto operator<=(const Resource& rhs_resource) const -> bool {
            return this->dominance_function_->check_dominance(*this, rhs_resource);
        }

        /// @brief Whether this composed backward label dominates @p rhs_resource, by the dominance
        ///        function's backward rule.
        ///
        /// @param rhs_resource The resource to compare against.
        /// @return `true` if this resource backward-dominates @p rhs_resource.
        [[nodiscard]] auto back_dominates(const Resource& rhs_resource) const -> bool {
            return this->dominance_function_->check_back_dominance(*this, rhs_resource);
        }

        /// @brief The promise on the cost: an @c eps such that `*this <= other` implies
        ///        `get_cost() <= other.get_cost() + eps`; empty when there is none.
        ///
        /// The dominance function's own (@c DominanceFunction::cost_order_tolerance) when it
        /// declares one; otherwise, when a dominance needs every component's
        /// (@c DominanceFunction::requires_component_dominance), the cost function's inference
        /// from the components (@c CostFunction::component_wise_cost_order_tolerance).
        ///
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] auto cost_order_tolerance() const -> std::optional<double> {
            if (auto declared = this->dominance_function_->cost_order_tolerance()) {
                return declared;
            }
            return inferred_cost_order_tolerance(/*backward=*/false);
        }

        /// @brief The same promise for @ref back_dominates: the dominance function's
        ///        @c DominanceFunction::back_cost_order_tolerance, or else the inference, from the
        ///        components' backward dominance.
        ///
        /// @return The tolerance, or @c std::nullopt.
        [[nodiscard]] auto back_cost_order_tolerance() const -> std::optional<double> {
            if (auto declared = this->dominance_function_->back_cost_order_tolerance()) {
                return declared;
            }
            return inferred_cost_order_tolerance(/*backward=*/true);
        }

        /// @brief The dominance function's type, as a message names it.
        [[nodiscard]] auto dominance_function_name() const -> std::string {
            return short_type_name(*this->dominance_function_);
        }

        /// @brief Where @ref cost_order_tolerance (or, @p backward, @ref
        ///        back_cost_order_tolerance) comes from, as a message names it: the dominance
        ///        function's declaration, or the inference from the cost function.
        ///
        /// @param backward Whether to name the backward promise's source.
        [[nodiscard]] auto cost_order_source(bool backward) const -> std::string {
            const bool declared =
                backward ? this->dominance_function_->back_cost_order_tolerance().has_value()
                         : this->dominance_function_->cost_order_tolerance().has_value();
            if (declared) {
                return "the dominance function " + dominance_function_name() +
                       (backward ? " (back_cost_order_tolerance)" : " (cost_order_tolerance)");
            }
            return "the cost order inferred for the dominance function " +
                   dominance_function_name() + " from the cost function " +
                   short_type_name(*this->cost_function_) +
                   (backward ? " (component_wise_cost_order_tolerance, backward)"
                             : " (component_wise_cost_order_tolerance)") +
                   " and the components' value_order_tolerance";
        }

        /// @brief Returns the total cost of this composed resource.
        ///
        /// The cost function is evaluated once after the components change, and the value is
        /// kept until they change again: a label's cost is read many times (by every dominance
        /// test, the schedules, the take rule), and with a @c CompositionCostFunction each
        /// evaluation is a virtual call per component. A change is anything that hands out a
        /// mutable component (see @c Composition::kept_cost_invalid), an extension
        /// (@c Extender::extend writes into this resource) and @ref reset. The cost function must
        /// therefore depend on the components only, and so must each component's own cost function
        /// when it adds their values. Call @ref invalidate_cost when it reads anything else that
        /// changes, when a component is written through a const composition, and when it is
        /// written through a reference obtained before a later call: the change is recorded when a
        /// mutable component is handed out, not when it is written. A call
        /// may store the value: two threads must not call it on the same resource at once (a
        /// search reads its labels from one thread).
        ///
        /// Built with @c RCSPP_VERIFY_COST_CACHE (@ref kVerifyCostCache: the Debug builds of the
        /// test suite), a call that would return the kept value first evaluates the cost function,
        /// and throws @c std::logic_error when the two differ: a change that did not invalidate it.
        ///
        /// @return The cost value computed by the cost function.
        /// @throws std::logic_error With @c RCSPP_VERIFY_COST_CACHE, on a stale kept value.
        [[nodiscard]] auto get_cost() const -> double {
            if constexpr (kVerifyCostCache) {
                verify_kept_cost();
            }
            if (this->kept_cost_invalid()) {
                cached_cost_ = this->cost_function_->get_cost(*this);
                this->mark_cost_kept();
            }
            return cached_cost_;
        }

        /// @brief Forgets the kept cost, so that the next @ref get_cost evaluates the cost
        ///        function.
        ///
        /// Needed only when the cost changes without a mutable access to the components: a
        /// component written through a const composition, or a cost function whose result
        /// depends on something else that changed.
        void invalidate_cost() const noexcept { this->invalidate_kept_cost(); }

        /// @brief Whether the cost is additive: the cost of a path is the sum of its two halves'
        ///        costs, wherever it is split.
        ///
        /// @return Result of the cost function's `is_additive` check.
        [[nodiscard]] auto is_cost_additive() const -> bool {
            return this->cost_function_->is_additive(*this);
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

        /// @brief Whether this forward label and @p back_resource fit together: every component
        ///        must accept the pair.
        ///
        /// @param back_resource The backward resource to join with.
        /// @return Result of the feasibility function's `can_be_joined` check.
        [[nodiscard]] auto can_be_joined(const Resource& back_resource) const -> bool {
            return this->feasibility_function_->can_be_joined(*this, back_resource);
        }

    private:
        /// @brief With @ref kVerifyCostCache: throws when the tracking says the kept cost is
        ///        current but the cost function gives another value.
        ///
        /// A kept cost that differs means a component changed without invalidating it. When the
        /// kept cost is already invalid, @ref get_cost evaluates it anyway, so
        /// there is nothing to check.
        void verify_kept_cost() const {
            if (this->kept_cost_invalid()) {
                return;
            }
            const double computed = this->cost_function_->get_cost(*this);
            if (computed != cached_cost_ && !(std::isnan(computed) && std::isnan(cached_cost_))) {
                throw std::logic_error(
                    "Resource::get_cost: the kept cost " + std::to_string(cached_cost_) +
                    " differs from the " + std::to_string(computed) + " of the cost function " +
                    short_type_name(*this->cost_function_) +
                    ": a component changed without invalidating the kept cost (see "
                    "invalidate_cost)");
            }
        }

        /// @brief The cost order inferred from the components, when the dominance function needs
        ///        every component's dominance; empty otherwise.
        ///
        /// @param backward Whether to infer it for @ref back_dominates.
        [[nodiscard]] auto inferred_cost_order_tolerance(bool backward) const
            -> std::optional<double> {
            if (!this->dominance_function_->requires_component_dominance()) {
                return std::nullopt;
            }
            return this->cost_function_->component_wise_cost_order_tolerance(*this, backward);
        }

        // The cost function's value for the components as they were when it was last evaluated;
        // valid while Composition::kept_cost_invalid() is false (see get_cost).
        mutable double cached_cost_ = 0.0;

    public:
        /// @brief Returns `true` if this resource can reach the given destination node.
        ///
        /// @param destination_node_id Identifier of the destination node.
        /// @return Result of the feasibility function's `is_reachable` check.
        [[nodiscard]] auto is_reachable(size_t destination_node_id) const -> bool {
            return this->feasibility_function_->is_reachable(*this, destination_node_id);
        }
};

}  // namespace rcspp
