// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/base/resource_prototype.hpp"
#include "rcspp/resource/composition/composition.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"

namespace rcspp {
template <typename... ResourceTypes>
    requires(std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class Resource<ResourceBaseComposition<ResourceTypes...>>
    : public ResourcePrototype<Resource<ResourceBaseComposition<ResourceTypes...>>,
                               ResourceBaseComposition<ResourceTypes...>>,
      public Composition<Resource, ResourceTypes...> {
        using Prototype = ResourcePrototype<Resource, ResourceBaseComposition<ResourceTypes...>>;

    public:
        Resource() = default;

        Resource(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            std::unique_ptr<DominanceFunction<ResourceBaseComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceBaseComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id),
              Composition<Resource, ResourceTypes...>(std::move(resource_components)) {}

        Resource(
            std::unique_ptr<DominanceFunction<ResourceBaseComposition<ResourceTypes...>>>
                dominance_function,
            std::unique_ptr<FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceBaseComposition<ResourceTypes...>>> cost_function,
            std::size_t node_id = 0)
            : Prototype(std::move(dominance_function), std::move(feasibility_function),
                        std::move(cost_function), node_id) {}

        Resource(
            std::tuple<std::vector<std::unique_ptr<Resource<ResourceTypes>>>...>
                resource_components,
            DominanceFunction<ResourceBaseComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceBaseComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Prototype(dominance_function, feasibility_function, cost_function, node_id),
              Composition<Resource, ResourceTypes...>(std::move(resource_components)) {}

        Resource(
            DominanceFunction<ResourceBaseComposition<ResourceTypes...>>* dominance_function,
            FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>* feasibility_function,
            CostFunction<ResourceBaseComposition<ResourceTypes...>>* cost_function,
            std::size_t node_id = 0)
            : Prototype(dominance_function, feasibility_function, cost_function, node_id) {}

        // Deep-copies both the prototype (function objects) and the composition (component
        // resources).
        Resource(Resource const& rhs_resource)
            : Prototype(rhs_resource), Composition<Resource, ResourceTypes...>(rhs_resource) {}

        Resource(Resource&& rhs_resource) noexcept
            : Prototype(), Composition<Resource, ResourceTypes...>() {
            swap(*this, rhs_resource);
        }

        auto operator=(Resource rhs_resource) -> auto& {
            swap(*this, rhs_resource);
            return *this;
        }

        friend void swap(Resource& first, Resource& second) {
            using std::swap;
            swap(static_cast<Prototype&>(first), static_cast<Prototype&>(second));
            swap(static_cast<Composition<Resource, ResourceTypes...>&>(first),
                 static_cast<Composition<Resource, ResourceTypes...>&>(second));
        }

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
};

}  // namespace rcspp
