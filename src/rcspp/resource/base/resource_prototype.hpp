// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_type.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

template <typename ResourceClass, typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ResourcePrototype {
    public:
        ResourcePrototype()
            : value_(),
              unique_dominance_function_(nullptr),
              unique_feasibility_function_(nullptr),
              unique_cost_function_(nullptr),
              dominance_function_(nullptr),
              feasibility_function_(nullptr),
              cost_function_(nullptr),
              node_id_(0) {}

        ResourcePrototype(const ResourceType& resource_value,
                          std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::size_t node_id = 0)
            : value_(resource_value),
              unique_dominance_function_(std::move(dominance_function)),
              unique_feasibility_function_(std::move(feasibility_function)),
              unique_cost_function_(std::move(cost_function)),
              dominance_function_(unique_dominance_function_.get()),
              feasibility_function_(unique_feasibility_function_.get()),
              cost_function_(unique_cost_function_.get()),
              node_id_(node_id) {}

        ResourcePrototype(ResourceType&& resource_value,
                          std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::size_t node_id = 0)
            : value_(std::move(resource_value)),
              unique_dominance_function_(std::move(dominance_function)),
              unique_feasibility_function_(std::move(feasibility_function)),
              unique_cost_function_(std::move(cost_function)),
              dominance_function_(unique_dominance_function_.get()),
              feasibility_function_(unique_feasibility_function_.get()),
              cost_function_(unique_cost_function_.get()),
              node_id_(node_id) {}

        ResourcePrototype(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::size_t node_id = 0)
            : value_(),
              unique_dominance_function_(std::move(dominance_function)),
              unique_feasibility_function_(std::move(feasibility_function)),
              unique_cost_function_(std::move(cost_function)),
              dominance_function_(unique_dominance_function_.get()),
              feasibility_function_(unique_feasibility_function_.get()),
              cost_function_(unique_cost_function_.get()),
              node_id_(node_id) {}

        ResourcePrototype(const ResourceType& resource_value,
                          DominanceFunction<ResourceType>* dominance_function,
                          FeasibilityFunction<ResourceType>* feasibility_function,
                          CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : value_(resource_value),
              dominance_function_(dominance_function),
              feasibility_function_(feasibility_function),
              cost_function_(cost_function),
              node_id_(node_id) {}

        ResourcePrototype(ResourceType&& resource_value,
                          DominanceFunction<ResourceType>* dominance_function,
                          FeasibilityFunction<ResourceType>* feasibility_function,
                          CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : value_(std::move(resource_value)),
              dominance_function_(dominance_function),
              feasibility_function_(feasibility_function),
              cost_function_(cost_function),
              node_id_(node_id) {}

        ResourcePrototype(DominanceFunction<ResourceType>* dominance_function,
                          FeasibilityFunction<ResourceType>* feasibility_function,
                          CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : value_(),
              dominance_function_(dominance_function),
              feasibility_function_(feasibility_function),
              cost_function_(cost_function),
              node_id_(node_id) {}

        explicit ResourcePrototype(ResourceClass const& rhs_resource)
            : value_(rhs_resource.value_),
              unique_dominance_function_(rhs_resource.unique_dominance_function_
                                             ? rhs_resource.unique_dominance_function_->clone()
                                             : nullptr),
              unique_feasibility_function_(rhs_resource.unique_feasibility_function_
                                               ? rhs_resource.unique_feasibility_function_->clone()
                                               : nullptr),
              unique_cost_function_(rhs_resource.unique_cost_function_
                                        ? rhs_resource.unique_cost_function_->clone()
                                        : nullptr),
              dominance_function_(unique_dominance_function_ ? unique_dominance_function_.get()
                                                             : rhs_resource.dominance_function_),
              feasibility_function_(unique_feasibility_function_
                                        ? unique_feasibility_function_.get()
                                        : rhs_resource.feasibility_function_),
              cost_function_(unique_cost_function_ ? unique_cost_function_.get()
                                                   : rhs_resource.cost_function_),
              node_id_(rhs_resource.get_node_id()) {}

        explicit ResourcePrototype(ResourceClass&& rhs_resource) : ResourcePrototype() {
            swap(*this, rhs_resource);
        }

        ~ResourcePrototype() = default;

        auto operator=(ResourceClass rhs_resource) -> ResourceClass& {
            swap(*this, rhs_resource);
            return downcast();
        }

        // To implement the copy-and-swap idiom
        friend void swap(ResourcePrototype& first, ResourcePrototype& second) noexcept {
            using std::swap;

            swap(first.value_, second.value_);

            // Swap the unique_ptr members that own the function objects
            swap(first.unique_dominance_function_, second.unique_dominance_function_);
            swap(first.unique_feasibility_function_, second.unique_feasibility_function_);
            swap(first.unique_cost_function_, second.unique_cost_function_);

            // Swap the raw pointers
            swap(first.dominance_function_, second.dominance_function_);
            swap(first.feasibility_function_, second.feasibility_function_);
            swap(first.cost_function_, second.cost_function_);
            swap(first.node_id_, second.node_id_);
        }

        [[nodiscard]] auto clone() const -> std::unique_ptr<ResourceClass> {
            return std::make_unique<ResourceClass>(downcast());
        }

        [[nodiscard]] auto get_node_id() const -> size_t { return node_id_; }

        // Read-only access to the stored resource value
        [[nodiscard]] auto get_value() const -> const ResourceType& { return value_; }

        // Mutable access — used internally (e.g. by ExtenderPrototype) to pass ResourceType*
        [[nodiscard]] auto get_value() -> ResourceType& { return value_; }

        // Forward set_value calls to the stored value (only valid when ResourceType has set_value)
        template <typename... Args>
        void set_value(Args&&... args) {
            value_.set_value(std::forward<Args>(args)...);
        }

        [[nodiscard]] auto create(const size_t node_id) const -> std::unique_ptr<ResourceClass> {
            auto new_resource =
                std::make_unique<ResourceClass>(unique_dominance_function_->create(node_id),
                                                unique_feasibility_function_->create(node_id),
                                                unique_cost_function_->create(node_id),
                                                node_id);

            return new_resource;
        }

        [[nodiscard]] auto create(const ResourceType& resource_value, const size_t node_id) const
            -> std::unique_ptr<ResourceClass> {
            auto new_resource =
                std::make_unique<ResourceClass>(resource_value,
                                                unique_dominance_function_->create(node_id),
                                                unique_feasibility_function_->create(node_id),
                                                unique_cost_function_->create(node_id),
                                                node_id);

            return new_resource;
        }

        // Create a new resource from a shallow copy of the current resource.
        [[nodiscard]] auto copy() const -> std::unique_ptr<ResourceClass> {
            auto new_resource = std::make_unique<ResourceClass>(dominance_function_,
                                                                feasibility_function_,
                                                                cost_function_,
                                                                node_id_);

            return new_resource;
        }

        void reset(const size_t node_id) {
            value_.reset();

            node_id_ = node_id;

            dominance_function_->reset(node_id);
            feasibility_function_->reset(node_id);
            cost_function_->reset(node_id);
        }

        // Reset the resource and copy the function objects from the resource passed as argument.
        void reset(const ResourceClass& resource) {
            value_.reset();

            node_id_ = resource.node_id_;

            dominance_function_ = resource.dominance_function_;
            feasibility_function_ = resource.feasibility_function_;
            cost_function_ = resource.cost_function_;
        }

    protected:
        ResourceType value_;

        std::unique_ptr<DominanceFunction<ResourceType>> unique_dominance_function_;
        std::unique_ptr<FeasibilityFunction<ResourceType>> unique_feasibility_function_;
        std::unique_ptr<CostFunction<ResourceType>> unique_cost_function_;

        DominanceFunction<ResourceType>* dominance_function_;
        FeasibilityFunction<ResourceType>* feasibility_function_;
        CostFunction<ResourceType>* cost_function_;

        size_t node_id_;

    private:
        [[nodiscard]] ResourceClass& downcast() { return static_cast<ResourceClass&>(*this); }

        [[nodiscard]] const ResourceClass& downcast() const {
            return static_cast<ResourceClass const&>(*this);
        }
};

}  // namespace rcspp
