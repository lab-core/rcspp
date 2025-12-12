// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"

namespace rcspp {
template <typename... ResourceTypes>
class ResourceComposition
    : public Resource<ResourceBaseComposition<ResourceTypes...>>,  // for the composition
      public Composition<Resource<ResourceTypes>...> {  // for each component of the composition
        friend class ResourceCompositionFactory<ResourceTypes...>;

    public:
        ResourceComposition() = default;

        ResourceComposition(
            const ResourceBaseComposition<ResourceTypes...>& resource_base,
            std::unique_ptr<DominanceFunction<ResourceBaseComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceBaseComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(
                  resource_base, std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function)) {}

        ResourceComposition(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            std::unique_ptr<DominanceFunction<ResourceBaseComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceBaseComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), node_id),
              Composition<Resource<ResourceTypes>...>(std::move(resource_components)) {}

        ResourceComposition(
            std::unique_ptr<DominanceFunction<ResourceBaseComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceBaseComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), node_id) {}

        ResourceComposition(
            const ResourceBaseComposition<ResourceTypes...>& resource_base,
            DominanceFunction<ResourceBaseComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceBaseComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), node_id) {}

        ResourceComposition(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            DominanceFunction<ResourceBaseComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceBaseComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), node_id),
              Composition<Resource<ResourceTypes>...>(std::move(resource_components)) {}

        ResourceComposition(
            DominanceFunction<ResourceBaseComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceBaseComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(
                  std::move(dominance_function), std::move(feasibility_function),
                  std::move(cost_function), node_id) {}

        ResourceComposition(ResourceComposition const& rhs_resource)
            : Resource<ResourceBaseComposition<ResourceTypes...>>(rhs_resource),
              Composition<Resource<ResourceTypes>...>(rhs_resource) {}

        ResourceComposition(ResourceComposition&& rhs_resource) { swap(*this, rhs_resource); }

        auto operator=(ResourceComposition rhs_resource) -> auto& {
            Resource<ResourceBaseComposition<ResourceTypes...>>::swap(*this, rhs_resource);
            return *this;
        }

        [[nodiscard]] auto clone_resource() const -> auto {
            auto res_ptr = std::make_unique<ResourceComposition>(
                static_cast<const ResourceComposition&>(*this));
            return res_ptr;
        }

        [[nodiscard]] auto create(const size_t node_id) const -> auto {
            // Create a resource based on the resources contained in a single vector of resources.
            auto create_res_vec_function = [&](const auto& sing_res_vec,
                                               auto& sing_new_res_vec) -> auto {
                std::transform(sing_res_vec.begin(),
                               sing_res_vec.end(),
                               std::back_inserter(sing_new_res_vec),
                               [node_id](const auto& res) { return res->create(node_id); });
            };
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                new_resource_components;
            this->apply(new_resource_components, create_res_vec_function);

            return std::make_unique<ResourceComposition>(
                std::move(new_resource_components),
                this->dominance_function_->create(node_id),
                this->feasibility_function_->create(node_id),
                this->cost_function_->create(node_id),
                node_id);
        }

        [[nodiscard]] auto copy() const -> std::unique_ptr<ResourceComposition> {
            // Copy a resource based on the resources contained in a single vector of resources.
            const auto copy_res_vec_function = [&](auto& sing_new_res_vec,
                                                   const auto& sing_res_vec) -> auto {
                std::transform(sing_res_vec.begin(),
                               sing_res_vec.end(),
                               std::back_inserter(sing_new_res_vec),
                               [](const auto& res) { return res->copy(); });
            };
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                new_resource_components;
            this->apply(new_resource_components, copy_res_vec_function);

            return std::make_unique<ResourceComposition>(std::move(new_resource_components),
                                                         this->dominance_function_,
                                                         this->feasibility_function_,
                                                         this->cost_function_,
                                                         this->node_id_);
        }

        void reset(size_t node_id) {
            Resource<ResourceBaseComposition<ResourceTypes...>>::reset(node_id);
            this->for_each_component([node_id](auto&& res) { res->reset(node_id); });
        }

        void reset(const Resource<ResourceBaseComposition<ResourceTypes...>>& other_resource) {
            Resource<ResourceBaseComposition<ResourceTypes...>>::reset(other_resource.node_id);
            const auto& other_composition = static_cast<const ResourceComposition&>(other_resource);
            this->for_each_component(other_composition, [](auto&& res_comp, auto&& other_res_comp) {
                res_comp.reset(other_res_comp);
            });
        }
};
}  // namespace rcspp
