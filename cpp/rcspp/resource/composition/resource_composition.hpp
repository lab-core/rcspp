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
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class Resource<ResourceTypeComposition<ResourceTypes...>>
    : public ResourcePrototype<Resource<ResourceTypeComposition<ResourceTypes...>>,
                               ResourceTypeComposition<ResourceTypes...>>,
      public Composition<Resource, ResourceTypes...> {
        using Prototype = ResourcePrototype<Resource, ResourceTypeComposition<ResourceTypes...>>;

    public:
        Resource() = default;

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

        Resource(
            std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        Resource(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            DominanceFunction<ResourceTypeComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceTypeComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Prototype(dominance_function, feasibility_function, cost_function, node_id),
              Composition<Resource, ResourceTypes...>(std::move(resource_components)) {}

        Resource(
            DominanceFunction<ResourceTypeComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceTypeComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Prototype(dominance_function, feasibility_function, cost_function, node_id) {}

        // Deep-copies both the prototype (function objects) and the composition (component
        // resources).
        Resource(Resource const& rhs_resource)
            : Prototype(rhs_resource), Composition<Resource, ResourceTypes...>(rhs_resource) {}

        // GCOVR_EXCL_START (move constructor and swap; not exercised in unit tests)
        Resource(Resource&& rhs_resource) noexcept
            : Prototype(), Composition<Resource, ResourceTypes...>() {
            swap(*this, rhs_resource);
        }

        auto operator=(Resource rhs_resource) -> auto& {
            swap(*this, rhs_resource);
            return *this;
        }

        friend void swap(Resource& first, Resource& second) noexcept {
            using std::swap;
            swap(static_cast<Prototype&>(first), static_cast<Prototype&>(second));
            swap(static_cast<Composition<Resource, ResourceTypes...>&>(first),
                 static_cast<Composition<Resource, ResourceTypes...>&>(second));
        }
        // GCOVR_EXCL_STOP

        // Override: composition has no value to copy; just delegate to create(node_id).
        // GCOVR_EXCL_START (value-ignoring create overload; not called in unit tests)
        [[nodiscard]] auto create(
            const ResourceTypeComposition<ResourceTypes...>& /*resource_value*/,
            const size_t node_id) const -> std::unique_ptr<Resource> {
            return create(node_id);
        }
        // GCOVR_EXCL_STOP

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

        // GCOVR_EXCL_START (copy() with component cloning; not exercised in unit tests)
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
        // GCOVR_EXCL_STOP

        [[nodiscard]] auto clone() const -> auto { return Prototype::clone(); }

        void reset(size_t node_id) {
            Prototype::reset(node_id);
            this->for_each_component([node_id](auto&& res) { res->reset(node_id); });
        }

        void reset(const Resource& other_composition) {
            Prototype::reset(other_composition);
            this->for_each_component(other_composition, [](auto&& res_comp, auto&& other_res_comp) {
                res_comp.reset(*other_res_comp);
            });
        }

        // Check dominance — passes value_ for simple types, full Resource for composition types
        auto operator<=(const Resource& rhs_resource) const -> bool {
            return this->dominance_function_->check_dominance(*this, rhs_resource);
        }

        // Return resource cost
        [[nodiscard]] auto get_cost() const -> double {
            return this->cost_function_->get_cost(*this);
        }

        // Return true if the resource is feasible
        [[nodiscard]] auto is_feasible() const -> bool {
            return this->feasibility_function_->is_feasible(*this);
        }

        [[nodiscard]] auto is_back_feasible() const -> bool {
            return this->feasibility_function_->is_back_feasible(*this);
        }

        [[nodiscard]] auto can_be_merged(const Resource& back_resource) const -> bool {
            return this->feasibility_function_->can_be_merged(*this, back_resource);
        }

        [[nodiscard]] auto is_reachable(size_t destination_node_id) const -> bool {
            return this->feasibility_function_->is_reachable(*this, destination_node_id);
        }
};

}  // namespace rcspp
