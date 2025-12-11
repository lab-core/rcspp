// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "../../utils/logger.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class ResourceCompositionFactory;

template <typename ResourceType>
class Resource : public ResourceType {
    public:
        Resource()
            : unique_dominance_function_(nullptr),
              unique_feasibility_function_(nullptr),
              unique_cost_function_(nullptr),
              dominance_function_(nullptr),
              feasibility_function_(nullptr),
              cost_function_(nullptr),
              node_id_(0) {}

        Resource(const ResourceType& resource_base,
                 std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : ResourceType(resource_base),
              unique_dominance_function_(std::move(dominance_function)),
              unique_feasibility_function_(std::move(feasibility_function)),
              unique_cost_function_(std::move(cost_function)),
              dominance_function_(unique_dominance_function_.get()),
              feasibility_function_(unique_feasibility_function_.get()),
              cost_function_(unique_cost_function_.get()),
              node_id_(node_id) {}

        Resource(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                 std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                 std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id = 0)
            : unique_dominance_function_(std::move(dominance_function)),
              unique_feasibility_function_(std::move(feasibility_function)),
              unique_cost_function_(std::move(cost_function)),
              dominance_function_(unique_dominance_function_.get()),
              feasibility_function_(unique_feasibility_function_.get()),
              cost_function_(unique_cost_function_.get()),
              node_id_(node_id) {}

        Resource(const ResourceType& resource_base,
                 DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : ResourceType(resource_base),
              dominance_function_(std::move(dominance_function)),
              feasibility_function_(std::move(feasibility_function)),
              cost_function_(std::move(cost_function)),
              node_id_(node_id) {}

        Resource(DominanceFunction<ResourceType>* dominance_function,
                 FeasibilityFunction<ResourceType>* feasibility_function,
                 CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : dominance_function_(std::move(dominance_function)),
              feasibility_function_(std::move(feasibility_function)),
              cost_function_(std::move(cost_function)),
              node_id_(node_id) {}

        Resource(Resource const& rhs_resource)
            : ResourceType(rhs_resource),
              unique_dominance_function_(rhs_resource.unique_dominance_function_->clone()),
              unique_feasibility_function_(rhs_resource.unique_feasibility_function_->clone()),
              unique_cost_function_(rhs_resource.unique_cost_function_->clone()),
              dominance_function_(unique_dominance_function_.get()),
              feasibility_function_(unique_feasibility_function_.get()),
              cost_function_(unique_cost_function_.get()),
              node_id_(rhs_resource.get_node_id()) {}

        Resource(Resource&& rhs_resource) : Resource() { swap(*this, rhs_resource); }

        ~Resource() override = default;

        auto operator=(Resource rhs_resource) -> Resource& {
            swap(*this, rhs_resource);

            return *this;
        }

        // To implement the copy-and-swap idiom
        friend void swap(Resource& first, Resource& second) {
            using std::swap;

            swap(first.dominance_function_, second.dominance_function_);
            swap(first.feasibility_function_, second.feasibility_function_);
            swap(first.cost_function_, second.cost_function_);
            swap(first.node_id_, second.node_id_);
        }

        // Check dominance
        auto operator<=(const Resource& rhs_resource) const -> bool {
            return dominance_function_->check_dominance(*this, rhs_resource);
        }

        // Return resource cost
        [[nodiscard]] auto get_cost() const -> double { return cost_function_->get_cost(*this); }

        // Return true if the resource is feasible
        [[nodiscard]] auto is_feasible() const -> bool {
            return feasibility_function_->is_feasible(*this);
        }

        [[nodiscard]] auto is_back_feasible() const -> bool {
            return feasibility_function_->is_back_feasible(*this);
        }

        [[nodiscard]] auto can_be_merged(const Resource<ResourceType>& back_resource) const
            -> bool {
            return feasibility_function_->can_be_merged(*this, back_resource);
        }

        [[nodiscard]] auto clone_resource() const -> std::unique_ptr<Resource<ResourceType>> {
            return std::make_unique<Resource<ResourceType>>(
                static_cast<Resource<ResourceType> const&>(*this));
        }

        [[nodiscard]] auto get_node_id() const -> size_t { return node_id_; }

        [[nodiscard]] auto create(const size_t node_id) const
            -> std::unique_ptr<Resource<ResourceType>> {
            auto new_resource =
                std::make_unique<Resource>(unique_dominance_function_->create(node_id),
                                           unique_feasibility_function_->create(node_id),
                                           unique_cost_function_->create(node_id),
                                           node_id);

            return new_resource;
        }

        [[nodiscard]] auto create(const ResourceType& resource_base, const size_t node_id) const
            -> std::unique_ptr<Resource<ResourceType>> {
            auto new_resource =
                std::make_unique<Resource>(resource_base,
                                           unique_dominance_function_->create(node_id),
                                           unique_feasibility_function_->create(node_id),
                                           unique_cost_function_->create(node_id),
                                           node_id);

            return new_resource;
        }

        // Create a new resource from a shallow copy of the current ressource.
        [[nodiscard]] auto copy() const -> std::unique_ptr<Resource<ResourceType>> {
            auto new_resource = std::make_unique<Resource>(dominance_function_,
                                                           feasibility_function_,
                                                           cost_function_,
                                                           node_id_);

            return new_resource;
        }

        void reset(const size_t node_id) {
            // Reset the associated ResourceBase.
            ResourceType::reset();

            node_id_ = node_id;

            dominance_function_->reset(node_id);
            feasibility_function_->reset(node_id);
            cost_function_->reset(node_id);
        }

        // Reset the resource and copy the function objects from the resource passed as argument.
        void reset(const Resource<ResourceType>& resource) {
            // Reset the associated ResourceBase.
            ResourceType::reset();

            node_id_ = resource.node_id_;

            dominance_function_ = resource.dominance_function_;
            feasibility_function_ = resource.feasibility_function_;
            cost_function_ = resource.cost_function_;
        }

    protected:
        std::unique_ptr<DominanceFunction<ResourceType>> unique_dominance_function_;
        std::unique_ptr<FeasibilityFunction<ResourceType>> unique_feasibility_function_;
        std::unique_ptr<CostFunction<ResourceType>> unique_cost_function_;

        DominanceFunction<ResourceType>* dominance_function_;
        FeasibilityFunction<ResourceType>* feasibility_function_;
        CostFunction<ResourceType>* cost_function_;

        size_t node_id_;
};
}  // namespace rcspp
