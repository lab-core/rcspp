// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <utility>

#include "rcspp/resource/base/resource_prototype.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

// Definition of ResourcePrototype for Resource
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Resource : public ResourcePrototype<Resource<ResourceType>, ResourceType> {
        using Prototype = ResourcePrototype<Resource, ResourceType>;

    public:
        Resource() = default;  // GCOVR_EXCL_LINE

        // GCOVR_EXCL_START — const-ref value constructor overload not exercised in tests
        Resource(const ResourceType& resource_value,
                 std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : Prototype(resource_value, std::move(dominance_function),
                        std::move(feasibility_function), std::move(cost_function), node_id) {}
        // GCOVR_EXCL_STOP

        Resource(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        Resource(const ResourceType& resource_value,
                 DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : Prototype(resource_value, std::move(dominance_function),
                        std::move(feasibility_function), std::move(cost_function), node_id) {}

        Resource(DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        Resource(Resource const& rhs_resource) : Prototype(rhs_resource) {}

        Resource(Resource&& rhs_resource) noexcept : Prototype(std::move(rhs_resource)) {}

        static void swap(Resource& first, Resource& second) noexcept {
            ResourcePrototype<Resource, ResourceType>::swap(first, second);
        }

        // Check dominance — passes value_ for simple types, full Resource for composition types
        auto operator<=(const Resource& rhs_resource) const -> bool {
            return this->dominance_function_->check_dominance(this->value_, rhs_resource.value_);
        }

        // Check distance from the resource to another
        [[nodiscard]] auto is_lower(const Resource& rhs_resource, double delta = 0) const -> bool {
            return this->dominance_function_->fast_check_dominance(this->value_,
                                                                   rhs_resource.value_,
                                                                   delta);
        }

        // Return resource cost
        [[nodiscard]] auto get_cost() const -> double {
            return this->cost_function_->get_cost(this->value_);
        }

        // Return true if the resource is feasible
        [[nodiscard]] auto is_feasible() const -> bool {
            return this->feasibility_function_->is_feasible(this->value_);
        }

        [[nodiscard]] auto is_back_feasible() const -> bool {
            return this->feasibility_function_->is_back_feasible(this->value_);
        }

        [[nodiscard]] auto can_be_merged(const Resource& back_resource) const -> bool {
            return this->feasibility_function_->can_be_merged(this->value_, back_resource.value_);
        }

        [[nodiscard]] auto is_reachable(size_t destination_node_id) const -> bool {
            return this->feasibility_function_->is_reachable(*this, destination_node_id);
        }
};
}  // namespace rcspp
