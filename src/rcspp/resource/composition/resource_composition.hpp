// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>  // NOLINT(build/include_order)
#include <iterator>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_base.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace rcspp {

template <typename... ResourceTypes>
  requires(std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class ResourceComposition : public ResourceBase<ResourceComposition<ResourceTypes...>> {
    template <typename... Types>
    friend class ResourceCompositionFactory;

  public:
    ResourceComposition() = default;

    explicit ResourceComposition(
      std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_base_components)
        : resource_base_components_(std::move(resource_base_components)) {}

    // Copy constructor
    ResourceComposition(const ResourceComposition& rhs_resource_composition) {
      // Clone the resources contained in a single vector of resources.
      const auto clone_res_vec_function = [&](auto& sing_res_vec,
                                              const auto& rhs_sing_res_vec) -> auto {
        std::transform(rhs_sing_res_vec.begin(),
                       rhs_sing_res_vec.end(),
                       std::back_inserter(sing_res_vec),
                       [](const auto& rhs_res) { return rhs_res->clone(); });
      };

      // Apply clone_res_vec_function to each component of the tuple
      // resource_base_components_.
      std::apply(
        [&](auto&&... args_res_comp) -> auto {
          std::apply(
            [&](auto&&... args_rhs_res_comp) -> auto {
              (clone_res_vec_function(args_res_comp, args_rhs_res_comp), ...);
            },
            rhs_resource_composition.resource_base_components_);
        },
        resource_base_components_);
    }

    ResourceComposition(ResourceComposition&& rhs_resource_composition)
        : ResourceBase<ResourceComposition<ResourceTypes...>>() {
      swap(*this, rhs_resource_composition);
    }

    ~ResourceComposition() override = default;

    auto operator=(ResourceComposition rhs_resource_composition) -> ResourceComposition& {
      swap(*this, rhs_resource_composition);

      return *this;
    }

    // To implement the copy-and-swap idiom
    friend void swap(ResourceComposition& first, ResourceComposition& second) {
      using std::swap;
      swap(first.resource_base_components_, second.resource_base_components_);
    }

    // Add (move) the resource in argument to the right vector of resources (i.e.,
    // ResourceTypeIndex).
    template <size_t ResourceTypeIndex, typename ResourceType>
    auto add_component(std::unique_ptr<ResourceType> resource) -> ResourceType& {
      return *std::get<ResourceTypeIndex>(resource_base_components_)
                .emplace_back(std::move(resource));
    }

    // Construct a resource from a list of arguments associated with a constructor (of
    // ResourceType).
    template <size_t ResourceTypeIndex, typename ResourceType, typename TypeTuple>
    auto add_component(const TypeTuple& resource_initializer) -> ResourceType& {
      auto res_init_index = std::make_index_sequence<
        std::tuple_size_v<typename std::remove_reference_t<decltype(resource_initializer)>>>{};

      auto resource = make_single_resource<ResourceType>(resource_initializer, res_init_index);

      return *std::get<ResourceTypeIndex>(resource_base_components_)
                .emplace_back(std::move(resource));
    }

    [[nodiscard]] auto get_type_components()
      -> std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& {
      return resource_base_components_;
    }

    [[nodiscard]] auto get_type_components() const
      -> const std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...>& {
      return resource_base_components_;
    }

    template <size_t ResourceTypeIndex>
    [[nodiscard]] auto get_type_components() -> auto& {
      return std::get<ResourceTypeIndex>(resource_base_components_);
    }

    template <size_t ResourceTypeIndex>
    [[nodiscard]] auto get_type_components() const -> const auto& {
      return std::get<ResourceTypeIndex>(resource_base_components_);
    }

    template <size_t ResourceTypeIndex>
    [[nodiscard]] auto get_type_component(size_t resource_index) const -> const auto& {
      return *(std::get<ResourceTypeIndex>(resource_base_components_)[resource_index]);
    }

    template <typename ResourceType>
    [[nodiscard]] auto get_type_components() -> auto& {
      constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
      return get_type_components<ResourceTypeIndex>();
    }

    template <typename ResourceType>
    [[nodiscard]] auto get_type_components() const -> const auto& {
      constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
      return get_type_components<ResourceTypeIndex>();
    }

    template <typename ResourceType>
    [[nodiscard]] auto get_type_component(size_t resource_index) const -> const auto& {
      constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
      return get_type_component<ResourceTypeIndex>(resource_index);
    }

    void reset() override {
      std::apply(
        [&](auto&&... args_res_comp) -> auto { (reset_resource_vector(&args_res_comp), ...); },
        resource_base_components_);
    }

  private:
    template <typename ResourceType>
    static void reset_resource_vector(
      std::vector<std::unique_ptr<ResourceType>>* resource_vector_ptr) {
      for (auto& res : *resource_vector_ptr) {
        (*res).reset();
      }
    }

    // Tuple of resource vectors in which each component of the tuple is associated with
    // a different type of resources from the template arguments (i.e., ResourceTypes...)
    std::tuple<std::vector<std::unique_ptr<ResourceTypes>>...> resource_base_components_;
};
}  // namespace rcspp
