// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/resource/base/resource_factory.hpp"
#include "rcspp/resource/composition/functions/cost/composition_cost_function.hpp"
#include "rcspp/resource/composition/functions/dominance/composition_dominance_function.hpp"
#include "rcspp/resource/composition/functions/expansion/composition_expansion_function.hpp"
#include "rcspp/resource/composition/functions/feasibility/composition_feasibility_function.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"

namespace rcspp {

template <typename... ResourceTypes>
// requires (std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class ResourceCompositionFactory : public ResourceFactory<ResourceComposition<ResourceTypes...>> {
    public:
        ResourceCompositionFactory() = default;

        ResourceCompositionFactory(
            std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>>
                expansion_function,
            std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>>
                dominance_function)
            : ResourceFactory<ResourceComposition<ResourceTypes...>>(
                  std::move(expansion_function), std::move(feasibility_function),
                  std::move(cost_function), std::move(dominance_function)) {}

        std::unique_ptr<Resource<ResourceComposition<ResourceTypes...>>> make_resource() override {
            return ResourceFactory<ResourceComposition<ResourceTypes...>>::make_resource();
        }

        std::unique_ptr<Resource<ResourceComposition<ResourceTypes...>>> make_resource(
            size_t node_id) override {
            return ResourceFactory<ResourceComposition<ResourceTypes...>>::make_resource(node_id);
        }

        template <typename... TypeTuples>
        std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource_base(
            const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            auto new_resource_composition = make_resource();

            auto make_resource_function = [&](const auto& res_init_vec,
                                              auto& res_comp_vec,
                                              auto& res_fac_vec) {
                for (int i = 0; i < res_init_vec.size(); i++) {
                    const auto& res_init = res_init_vec[i];

                    const auto& res_comp =
                        res_comp_vec.emplace_back(res_fac_vec[i]->make_resource_base());

                    auto res_init_index = std::make_index_sequence<
                        std::tuple_size_v<typename std::remove_reference_t<decltype(res_init)>>>{};

                    set_value_single_resource(res_comp, res_init, res_init_index);
                }
            };

            std::apply(
                [&](auto&&... args_res_init_vec) {
                    std::apply(
                        [&](auto&&... args_res_comp_vec) {
                            std::apply(
                                [&](auto&&... args_res_fac_vec) {
                                    (make_resource_function(args_res_init_vec,
                                                            args_res_comp_vec,
                                                            args_res_fac_vec),
                                     ...);
                                },
                                resource_factory_components_);
                        },
                        new_resource_composition->resource_base_components_);
                },
                resource_initializer);

            return new_resource_composition;
        }

        std::unique_ptr<Expander<ResourceComposition<ResourceTypes...>>> make_expander(
            const ResourceComposition<ResourceTypes...>& resource_base, size_t arc_id) override {
            const auto& resource_base_components = resource_base.get_type_components();

            auto new_expander_resource_composition =
                ResourceFactory<ResourceComposition<ResourceTypes...>>::make_expander(arc_id);

            auto make_expander_function = [&](const auto& res_base_vec,
                                              const auto& res_fac_vec,
                                              auto& res_comp_vec) {
                for (int i = 0; i < res_base_vec.size(); i++) {
                    const auto& res_base = *res_base_vec[i];

                    const auto& res_fac = res_fac_vec[i];

                    res_comp_vec.emplace_back(std::move(res_fac->make_expander(res_base, arc_id)));
                }
            };

            std::apply(
                [&](auto&&... args_res_base_vec) {
                    std::apply(
                        [&](auto&&... args_res_fac_vec) {
                            std::apply(
                                [&](auto&&... args_res_comp_vec) {
                                    (make_expander_function(args_res_base_vec,
                                                            args_res_fac_vec,
                                                            args_res_comp_vec),
                                     ...);
                                },
                                new_expander_resource_composition->expander_components_);
                        },
                        resource_factory_components_);
                },
                resource_base_components);

            return new_expander_resource_composition;
        }

        // Add (move) the resource factory in argument to the right vector of resource factories
        // (i.e., ResourceTypeIndex).
        template <size_t ResourceTypeIndex, typename ResourceType>
        ResourceFactory<ResourceType>& add_resource_factory(
            std::unique_ptr<ResourceFactory<ResourceType>> resource_factory) {
            const auto& resource_factory_ref =
                std::get<ResourceTypeIndex>(resource_factory_components_)
                    .emplace_back(std::move(resource_factory));

            update_resource_prototype();

            return *resource_factory_ref;
        }

        template <typename... TypeTuples>
        void update_expander(
            Expander<ResourceComposition<ResourceTypes...>>* expander_resource_composition,
            const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            auto update_resource_function = [&](const auto& res_init_vec,
                                                const auto& res_comp_vec) {
                for (int i = 0; i < res_init_vec.size(); i++) {
                    const auto& res_init = res_init_vec[i];

                    const auto& res_comp = res_comp_vec[i];

                    auto res_init_index = std::make_index_sequence<
                        std::tuple_size_v<typename std::remove_reference_t<decltype(res_init)>>>{};

                    set_value_single_resource(res_comp, res_init, res_init_index);
                }
            };

            std::apply(
                [&](auto&&... args_res_init_vec) {
                    std::apply(
                        [&](auto&&... args_res_comp_vec) {
                            (update_resource_function(args_res_init_vec, args_res_comp_vec), ...);
                        },
                        expander_resource_composition->expander_components_);
                },
                resource_initializer);
        }

        template <typename TypeTuple, size_t ResourceTypeIndex>
        void update_expander(
            Expander<ResourceComposition<ResourceTypes...>>* expander_resource_composition,
            std::size_t resource_index, const TypeTuple& single_resource_initializer) {
            const auto& res_comp = std::get<ResourceTypeIndex>(
                expander_resource_composition->expander_components_)[resource_index];

            auto res_init_index = std::make_index_sequence<std::tuple_size_v<
                typename std::remove_reference_t<decltype(single_resource_initializer)>>>{};

            set_value_single_resource(res_comp, single_resource_initializer, res_init_index);
        }

    private:
        std::tuple<std::vector<std::unique_ptr<ResourceFactory<ResourceTypes>>>...>
            resource_factory_components_;

        const Resource<ResourceComposition<ResourceTypes...>>& update_resource_prototype() {
            auto& prototype_resource_components =
                this->resource_prototype_->get_resource_components();

            auto update_prototype_function = [&](const auto& res_fac_vec, auto& prot_res_comp_vec) {
                prot_res_comp_vec.clear();

                for (int i = 0; i < res_fac_vec.size(); i++) {
                    const auto& res_fac = res_fac_vec[i];

                    prot_res_comp_vec.emplace_back(std::move(res_fac->make_resource()));
                }
            };

            std::apply(
                [&](auto&&... args_prot_res_comp_vec) {
                    std::apply(
                        [&](auto&&... args_res_fac_vec) {
                            (update_prototype_function(args_res_fac_vec, args_prot_res_comp_vec),
                             ...);
                        },
                        resource_factory_components_);
                },
                prototype_resource_components);

            return *(this->resource_prototype_);
        }

        template <typename ResourceType, typename TypeTuple, std::size_t... N>
        auto set_value_single_resource(const ResourceType& resource,
                                       const TypeTuple& resource_initializer,
                                       std::index_sequence<N...> /*unused*/) {
            resource->set_value(std::get<N>(resource_initializer)...);
        }
};
}  // namespace rcspp
