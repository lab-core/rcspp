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

/// @brief CRTP base class that provides common state and behaviour for resource objects.
///
/// `ResourcePrototype` stores the resource value together with its associated dominance,
/// feasibility, and cost function objects.  It implements the copy-and-swap assignment
/// idiom and offers factory methods (`create`, `copy`, `clone`) that derived classes
/// inherit without duplication.
///
/// Ownership of the three function objects can be either *exclusive* (via `unique_ptr`
/// members) or *shared/borrowed* (via raw-pointer members).  Both ownership modes are
/// supported by separate constructor overloads so that labels can cheaply share
/// per-node function objects created once by the graph builder.
///
/// @tparam ResourceClass  The concrete derived class (CRTP parameter).
/// @tparam ResourceType   The value type stored inside the resource; must satisfy
///                        `ResourceTypeConcept`.
template <typename ResourceClass, typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class ResourcePrototype {
    public:
        /// @brief Default constructor.
        ///
        /// Constructs a resource with a default-initialised value and null function pointers.
        ResourcePrototype()
            : value_(),
              unique_dominance_function_(nullptr),
              unique_feasibility_function_(nullptr),
              unique_cost_function_(nullptr),
              dominance_function_(nullptr),
              feasibility_function_(nullptr),
              cost_function_(nullptr),
              node_id_(0) {}

        /// @brief Constructs a resource with a copied value and exclusively-owned function objects.
        ///
        /// @param resource_value       Initial resource value (copied).
        /// @param dominance_function   Owned dominance function for this resource.
        /// @param feasibility_function Owned feasibility function for this resource.
        /// @param cost_function        Owned cost function for this resource.
        /// @param node_id              Graph node associated with this resource (default 0).
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

        /// @brief Constructs a resource with a moved value and exclusively-owned function objects.
        ///
        /// @param resource_value       Initial resource value (moved).
        /// @param dominance_function   Owned dominance function for this resource.
        /// @param feasibility_function Owned feasibility function for this resource.
        /// @param cost_function        Owned cost function for this resource.
        /// @param node_id              Graph node associated with this resource (default 0).
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

        /// @brief Constructs a default-value resource with exclusively-owned function objects.
        ///
        /// @param dominance_function   Owned dominance function for this resource.
        /// @param feasibility_function Owned feasibility function for this resource.
        /// @param cost_function        Owned cost function for this resource.
        /// @param node_id              Graph node associated with this resource (default 0).
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

        /// @brief Constructs a resource with a copied value and borrowed (non-owning) function
        ///        objects.
        ///
        /// The caller is responsible for keeping the pointed-to function objects alive for the
        /// lifetime of this resource.
        ///
        /// @param resource_value       Initial resource value (copied).
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Graph node associated with this resource (default 0).
        ResourcePrototype(const ResourceType& resource_value,
                          DominanceFunction<ResourceType>* dominance_function,
                          FeasibilityFunction<ResourceType>* feasibility_function,
                          CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : value_(resource_value),
              dominance_function_(dominance_function),
              feasibility_function_(feasibility_function),
              cost_function_(cost_function),
              node_id_(node_id) {}

        /// @brief Constructs a resource with a moved value and borrowed (non-owning) function
        ///        objects.
        ///
        /// @param resource_value       Initial resource value (moved).
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Graph node associated with this resource (default 0).
        ResourcePrototype(ResourceType&& resource_value,
                          DominanceFunction<ResourceType>* dominance_function,
                          FeasibilityFunction<ResourceType>* feasibility_function,
                          CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : value_(std::move(resource_value)),
              dominance_function_(dominance_function),
              feasibility_function_(feasibility_function),
              cost_function_(cost_function),
              node_id_(node_id) {}

        /// @brief Constructs a default-value resource with borrowed (non-owning) function objects.
        ///
        /// @param dominance_function   Non-owning pointer to the dominance function.
        /// @param feasibility_function Non-owning pointer to the feasibility function.
        /// @param cost_function        Non-owning pointer to the cost function.
        /// @param node_id              Graph node associated with this resource (default 0).
        ResourcePrototype(DominanceFunction<ResourceType>* dominance_function,
                          FeasibilityFunction<ResourceType>* feasibility_function,
                          CostFunction<ResourceType>* cost_function, std::size_t node_id = 0)
            : value_(),
              dominance_function_(dominance_function),
              feasibility_function_(feasibility_function),
              cost_function_(cost_function),
              node_id_(node_id) {}

        /// @brief Copy constructor — deep-copies the value and clones owned function objects.
        ///
        /// If a function object is exclusively owned (via `unique_ptr`), it is cloned; otherwise
        /// the raw pointer is copied (borrowed ownership is preserved).
        ///
        /// @param rhs_resource Source resource to copy from.
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

        /// @brief Move constructor — transfers ownership via copy-and-swap.
        ///
        /// @param rhs_resource Source resource to move from.
        explicit ResourcePrototype(ResourceClass&& rhs_resource) : ResourcePrototype() {
            swap(*this, rhs_resource);
        }

        ~ResourcePrototype() = default;

        /// @brief Copy-and-swap assignment operator.
        ///
        /// @param rhs_resource Resource to assign (passed by value to elide an extra copy).
        /// @return Reference to `*this` as the derived `ResourceClass`.
        auto operator=(ResourceClass rhs_resource) -> ResourceClass& {
            swap(*this, rhs_resource);
            return downcast();
        }

        /// @brief Swaps all members of two `ResourcePrototype` objects without throwing.
        ///
        /// Required by the copy-and-swap assignment idiom.  Swaps the stored value, all
        /// owned and borrowed function-object pointers, and the node identifier.
        ///
        /// @param first  First resource to swap.
        /// @param second Second resource to swap.
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

        /// @brief Creates a deep copy of this resource, including cloned function objects.
        ///
        /// @return A heap-allocated clone of the concrete derived object.
        [[nodiscard]] auto clone() const -> std::unique_ptr<ResourceClass> {
            return std::make_unique<ResourceClass>(downcast());
        }

        /// @brief Returns the graph node identifier associated with this resource.
        ///
        /// @return Node identifier.
        [[nodiscard]] auto get_node_id() const -> size_t { return node_id_; }

        // Read-only access to the stored resource value
        /// @brief Returns a const reference to the stored resource value.
        ///
        /// @return Const reference to the resource value.
        [[nodiscard]] auto get_value() const -> const ResourceType& { return value_; }

        // Mutable access — used internally (e.g. by ExtenderPrototype) to pass ResourceType*
        /// @brief Returns a mutable reference to the stored resource value.
        ///
        /// @return Mutable reference to the resource value.
        [[nodiscard]] auto get_value() -> ResourceType& { return value_; }

        // Forward set_value calls to the stored value (only valid when ResourceType has set_value)
        /// @brief Forwards a value-setting call to the underlying resource-value object.
        ///
        /// Only valid when `ResourceType` itself exposes a `set_value` method.
        ///
        /// @tparam Args Argument types forwarded to `ResourceType::set_value`.
        /// @param args  Arguments forwarded to `ResourceType::set_value`.
        template <typename... Args>
        void set_value(Args&&... args) {
            value_.set_value(std::forward<Args>(args)...);
        }

        /// @brief Creates a new resource at the given node using cloned function objects.
        ///
        /// The returned resource has a default-initialised value and exclusively-owned
        /// function objects freshly created for `node_id`.
        ///
        /// @param node_id Target graph node identifier.
        /// @return A new heap-allocated resource for `node_id`.
        [[nodiscard]] auto create(const size_t node_id) const -> std::unique_ptr<ResourceClass> {
            auto new_resource =
                std::make_unique<ResourceClass>(unique_dominance_function_->create(node_id),
                                                unique_feasibility_function_->create(node_id),
                                                unique_cost_function_->create(node_id),
                                                node_id);

            return new_resource;
        }

        /// @brief Creates a new resource with the given value at the specified node.
        ///
        /// @param resource_value Initial value for the new resource (copied).
        /// @param node_id        Target graph node identifier.
        /// @return A new heap-allocated resource initialised with `resource_value`.
        [[nodiscard]] auto create(const ResourceType& resource_value,
                                  const size_t node_id) const  // NOLINT
            -> std::unique_ptr<ResourceClass> {
            auto new_resource =
                std::make_unique<ResourceClass>(resource_value,
                                                unique_dominance_function_->create(node_id),
                                                unique_feasibility_function_->create(node_id),
                                                unique_cost_function_->create(node_id),
                                                node_id);

            return new_resource;
        }

        /// @brief Creates a new resource that borrows (shares) this resource's function objects.
        ///
        /// The returned resource does **not** own the function objects; the caller must ensure
        /// that this resource outlives the returned copy.
        ///
        /// @return A heap-allocated resource with a shallow copy of the function pointers.
        [[nodiscard]] auto copy() const -> std::unique_ptr<ResourceClass> {
            auto new_resource = std::make_unique<ResourceClass>(dominance_function_,
                                                                feasibility_function_,
                                                                cost_function_,
                                                                node_id_);

            return new_resource;
        }

        /// @brief Resets this resource to the initial state for a given node.
        ///
        /// Resets the stored value and propagates the reset to all three function objects.
        ///
        /// @param node_id Graph node identifier to re-initialise for.
        void reset(const size_t node_id) {
            value_.reset();

            node_id_ = node_id;

            dominance_function_->reset(node_id);
            feasibility_function_->reset(node_id);
            cost_function_->reset(node_id);
        }

        // Reset the resource and copy the function objects from the resource passed as argument.
        /// @brief Resets the value and rebinds the function-object pointers from another resource.
        ///
        /// After this call, the three function pointers point to the function objects of
        /// `resource`.  No ownership transfer occurs.
        ///
        /// @param resource Source resource whose function pointers are adopted.
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
