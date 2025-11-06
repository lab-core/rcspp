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
#include "rcspp/resource/composition/resource_composition.hpp"
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
             std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id)
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
             std::unique_ptr<CostFunction<ResourceType>> cost_function, std::size_t node_id)
        : unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(node_id) {}

    Resource(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
             std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
             std::unique_ptr<CostFunction<ResourceType>> cost_function)
        : unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(0) {}

    Resource(std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
             std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
             std::unique_ptr<CostFunction<ResourceType>> cost_function,
             const ResourceType& resource_base_prototype)
        : ResourceType(resource_base_prototype),
          unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(0) {}

    Resource(const ResourceType& resource_base, DominanceFunction<ResourceType>* dominance_function,
             FeasibilityFunction<ResourceType>* feasibility_function,
             CostFunction<ResourceType>* cost_function, std::size_t node_id)
        : ResourceType(resource_base),
          dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(node_id) {}

    Resource(DominanceFunction<ResourceType>* dominance_function,
             FeasibilityFunction<ResourceType>* feasibility_function,
             CostFunction<ResourceType>* cost_function, std::size_t node_id)
        : dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(node_id) {}

    Resource(DominanceFunction<ResourceType>* dominance_function,
             FeasibilityFunction<ResourceType>* feasibility_function,
             CostFunction<ResourceType>* cost_function)
        : dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(0) {}

    Resource(DominanceFunction<ResourceType>* dominance_function,
             FeasibilityFunction<ResourceType>* feasibility_function,
             CostFunction<ResourceType>* cost_function, const ResourceType& resource_base_prototype)
        : ResourceType(resource_base_prototype),
          dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(0) {}

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

    [[nodiscard]] auto clone_resource() const -> std::unique_ptr<Resource<ResourceType>> {
      return std::make_unique<Resource<ResourceType>>(
        static_cast<Resource<ResourceType> const&>(*this));
    }

    [[nodiscard]] auto get_node_id() const -> size_t { return node_id_; }

    [[nodiscard]] auto create(const size_t node_id) const
      -> std::unique_ptr<Resource<ResourceType>> {
      auto new_resource = std::make_unique<Resource>(unique_dominance_function_->create(node_id),
                                                     unique_feasibility_function_->create(node_id),
                                                     unique_cost_function_->create(node_id),
                                                     node_id);

      return new_resource;
    }

    [[nodiscard]] auto create(const ResourceType& resource_base, const size_t node_id) const
      -> std::unique_ptr<Resource<ResourceType>> {
      auto new_resource = std::make_unique<Resource>(resource_base,
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

  private:
    std::unique_ptr<DominanceFunction<ResourceType>> unique_dominance_function_;
    std::unique_ptr<FeasibilityFunction<ResourceType>> unique_feasibility_function_;
    std::unique_ptr<CostFunction<ResourceType>> unique_cost_function_;

    DominanceFunction<ResourceType>* dominance_function_;
    FeasibilityFunction<ResourceType>* feasibility_function_;
    CostFunction<ResourceType>* cost_function_;

    size_t node_id_;
};

// Specialization for ResourceComposition
// TODO(patrick): Find a way to avoid code duplication
template <typename... ResourceTypes>
class Resource<ResourceComposition<ResourceTypes...>>
    : public ResourceComposition<ResourceTypes...> {
    friend class ResourceCompositionFactory<ResourceTypes...>;

  public:
    Resource()
        : unique_dominance_function_(nullptr),
          unique_feasibility_function_(nullptr),
          unique_cost_function_(nullptr),
          dominance_function_(nullptr),
          feasibility_function_(nullptr),
          cost_function_(nullptr),
          node_id_(0) {}

    Resource(
      const ResourceComposition<ResourceTypes...>& resource_base,
      std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function,
      std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
        feasibility_function,
      std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
      std::size_t node_id)
        : ResourceComposition<ResourceTypes...>(resource_base),
          unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(node_id) {}

    Resource(
      std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...> resource_components,
      std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function,
      std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
        feasibility_function,
      std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
      std::size_t node_id)
        : resource_components_(std::move(resource_components)),
          unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(node_id) {}

    Resource(
      std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function,
      std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
        feasibility_function,
      std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
      std::size_t node_id)
        : unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(node_id) {}

    Resource(
      std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function,
      std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
        feasibility_function,
      std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function)
        : unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(0) {}

    // TODO(patrick): Check if this constructor is necessary
    Resource(
      std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>> dominance_function,
      std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
        feasibility_function,
      std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
      [[maybe_unused]] const ResourceComposition<ResourceTypes...>& resource_base_prototype)
        : unique_dominance_function_(std::move(dominance_function)),
          unique_feasibility_function_(std::move(feasibility_function)),
          unique_cost_function_(std::move(cost_function)),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(0) {}

    Resource(const ResourceComposition<ResourceTypes...>& resource_base,
             DominanceFunction<ResourceComposition<ResourceTypes...>>* dominance_function,
             FeasibilityFunction<ResourceComposition<ResourceTypes...>>* feasibility_function,
             CostFunction<ResourceComposition<ResourceTypes...>>* cost_function,
             std::size_t node_id)
        : ResourceComposition<ResourceTypes...>(resource_base),
          dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(node_id) {}

    Resource(
      std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...> resource_components,
      DominanceFunction<ResourceComposition<ResourceTypes...>>* dominance_function,
      FeasibilityFunction<ResourceComposition<ResourceTypes...>>* feasibility_function,
      CostFunction<ResourceComposition<ResourceTypes...>>* cost_function, std::size_t node_id)
        : resource_components_(std::move(resource_components)),
          dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(node_id) {}

    Resource(DominanceFunction<ResourceComposition<ResourceTypes...>>* dominance_function,
             FeasibilityFunction<ResourceComposition<ResourceTypes...>>* feasibility_function,
             CostFunction<ResourceComposition<ResourceTypes...>>* cost_function,
             std::size_t node_id)
        : dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(node_id) {}

    Resource(DominanceFunction<ResourceComposition<ResourceTypes...>>* dominance_function,
             FeasibilityFunction<ResourceComposition<ResourceTypes...>>* feasibility_function,
             CostFunction<ResourceComposition<ResourceTypes...>>* cost_function)
        : dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(0) {}

    // TODO(patrick): Check if this constructor is necessary
    Resource(DominanceFunction<ResourceComposition<ResourceTypes...>>* dominance_function,
             FeasibilityFunction<ResourceComposition<ResourceTypes...>>* feasibility_function,
             CostFunction<ResourceComposition<ResourceTypes...>>* cost_function,
             [[maybe_unused]] const ResourceComposition<ResourceTypes...>& resource_base_prototype)
        : dominance_function_(std::move(dominance_function)),
          feasibility_function_(std::move(feasibility_function)),
          cost_function_(std::move(cost_function)),
          node_id_(0) {}

    Resource(Resource const& rhs_resource)
        : ResourceComposition<ResourceTypes...>(rhs_resource),
          unique_dominance_function_(rhs_resource.unique_dominance_function_->clone()),
          unique_feasibility_function_(rhs_resource.unique_feasibility_function_->clone()),
          unique_cost_function_(rhs_resource.unique_cost_function_->clone()),
          dominance_function_(unique_dominance_function_.get()),
          feasibility_function_(unique_feasibility_function_.get()),
          cost_function_(unique_cost_function_.get()),
          node_id_(rhs_resource.get_node_id()) {
      // Apply clone_res_vec_function to each component of the tuple
      // resource_base_components_.
      std::apply(
        [&](auto&&... args_res_comp) -> auto {
          std::apply(
            [&](auto&&... args_rhs_res_comp) -> auto {
              (clone_resource_vector(&args_res_comp, args_rhs_res_comp), ...);
            },
            rhs_resource.resource_components_);
        },
        resource_components_);
    }

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

    [[nodiscard]] auto clone_resource() const
      -> std::unique_ptr<Resource<ResourceComposition<ResourceTypes...>>> {
      auto res_ptr = std::make_unique<Resource<ResourceComposition<ResourceTypes...>>>(
        static_cast<Resource<ResourceComposition<ResourceTypes...>> const&>(*this));

      return res_ptr;
    }

    [[nodiscard]] auto get_node_id() const -> size_t { return node_id_; }

    [[nodiscard]] auto create(const size_t node_id) const
      -> std::unique_ptr<Resource<ResourceComposition<ResourceTypes...>>> {
      std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...> new_resource_components;

      // Create a resource based on the resources contained in a single vector of resources.
      const auto create_res_vec_function = [&](auto& sing_new_res_vec,
                                               const auto& sing_res_vec) -> auto {
        std::transform(sing_res_vec.begin(),
                       sing_res_vec.end(),
                       std::back_inserter(sing_new_res_vec),
                       [node_id](const auto& res) { return res->create(node_id); });
      };

      // Apply create_res_vec_function to each component of the tuple resource_components_.
      std::apply(
        [&](auto&&... args_new_res_comp) -> auto {
          std::apply(
            [&](auto&&... args_res_comp) -> auto {
              (create_res_vec_function(args_new_res_comp, args_res_comp), ...);
            },
            resource_components_);
        },
        new_resource_components);

      return std::make_unique<Resource>(std::move(new_resource_components),
                                        dominance_function_->create(node_id),
                                        feasibility_function_->create(node_id),
                                        cost_function_->create(node_id),
                                        node_id);
    }

    [[nodiscard]] auto copy() const
      -> std::unique_ptr<Resource<ResourceComposition<ResourceTypes...>>> {
      std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...> new_resource_components;

      // Apply create_res_vec_function to each component of the tuple resource_components_.
      std::apply(
        [&](auto&&... args_new_res_comp) -> auto {
          std::apply(
            [&](auto&&... args_res_comp) -> auto {
              (copy_resource_vector(&args_new_res_comp, args_res_comp), ...);
            },
            resource_components_);
        },
        new_resource_components);

      return std::make_unique<Resource>(std::move(new_resource_components),
                                        dominance_function_,
                                        feasibility_function_,
                                        cost_function_,
                                        node_id_);
    }

    // New methods

    [[nodiscard]] auto get_resource_components()
      -> std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>& {
      return resource_components_;
    }

    [[nodiscard]] auto get_resource_components() const
      -> const std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>& {
      return resource_components_;
    }

    template <size_t ResourceTypeIndex>
    [[nodiscard]] auto get_resource_components() -> auto& {
      return std::get<ResourceTypeIndex>(resource_components_);
    }

    template <size_t ResourceTypeIndex>
    [[nodiscard]] auto get_resource_components() const -> const auto& {
      return std::get<ResourceTypeIndex>(resource_components_);
    }

    template <size_t ResourceTypeIndex>
    [[nodiscard]] auto get_resource_component(size_t resource_index) const -> const auto& {
      return *(std::get<ResourceTypeIndex>(resource_components_)[resource_index]);
    }

    template <typename ResourceType>
    [[nodiscard]] auto get_resource_components() -> auto& {
      constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
      return get_resource_components<ResourceTypeIndex>();
    }

    template <typename ResourceType>
    [[nodiscard]] auto get_resource_components() const -> const auto& {
      constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
      return get_resource_components<ResourceTypeIndex>();
    }

    template <typename ResourceType>
    [[nodiscard]] auto get_resource_component(size_t resource_index) const -> const auto& {
      constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
      return get_resource_component<ResourceTypeIndex>(resource_index);
    }

    void reset(size_t node_id) {
      // LOG_TRACE(__FUNCTION__, '\n');

      node_id_ = node_id;

      auto reset_function = [&](auto& sing_res_vec) -> auto {
        for (auto& res : sing_res_vec) {
          res->reset(node_id);
        }
      };

      std::apply([&](auto&&... args_res_comp) -> auto { (reset_function(args_res_comp), ...); },
                 resource_components_);
    }

    void reset(const Resource<ResourceComposition<ResourceTypes...>>& resource) {
      // LOG_TRACE(__FUNCTION__, '\n');

      node_id_ = resource.node_id_;

      std::apply(
        [&](auto&&... args_res_comp) -> auto {
          std::apply(
            [&](auto&&... args_other_res_comp) -> auto {
              (reset_resource_vector(&args_res_comp, args_other_res_comp), ...);
            },
            resource.resource_components_);
        },
        resource_components_);
    }

  private:
    template <typename ResourceType>
    void clone_resource_vector(
      std::vector<std::unique_ptr<Resource<ResourceType>>>* resource_vector_to_ptr,
      const std::vector<std::unique_ptr<Resource<ResourceType>>>& resource_vec_from) const {
      std::transform(resource_vec_from.begin(),
                     resource_vec_from.end(),
                     std::back_inserter(*resource_vector_to_ptr),
                     [](const auto& res) { return res->clone_resource(); });
    }

    template <typename ResourceType>
    void copy_resource_vector(
      std::vector<std::unique_ptr<Resource<ResourceType>>>* resource_vector_to_ptr,
      const std::vector<std::unique_ptr<Resource<ResourceType>>>& resource_vec_from) const {
      std::transform(resource_vec_from.begin(),
                     resource_vec_from.end(),
                     std::back_inserter(*resource_vector_to_ptr),
                     [](const auto& res) { return res->copy(); });
    }

    template <typename ResourceType>
    void reset_resource_vector(
      std::vector<std::unique_ptr<Resource<ResourceType>>>* resource_vector_to_ptr,
      const std::vector<std::unique_ptr<Resource<ResourceType>>>& resource_vec_from) const {
      for (int i = 0; i < resource_vector_to_ptr->size(); i++) {
        (*resource_vector_to_ptr)[i]->reset(*resource_vec_from[i]);
      }
    }

    // New attribute
    std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...> resource_components_;

    std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>>
      unique_dominance_function_;
    std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
      unique_feasibility_function_;
    std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> unique_cost_function_;

    DominanceFunction<ResourceComposition<ResourceTypes...>>* dominance_function_;
    FeasibilityFunction<ResourceComposition<ResourceTypes...>>* feasibility_function_;
    CostFunction<ResourceComposition<ResourceTypes...>>* cost_function_;

    size_t node_id_;
};
}  // namespace rcspp
