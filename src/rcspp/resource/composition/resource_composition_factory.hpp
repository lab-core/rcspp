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
#include "rcspp/resource/composition/functions/extension/composition_extension_function.hpp"
#include "rcspp/resource/composition/functions/feasibility/composition_feasibility_function.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"

namespace rcspp {

template <typename... ResourceTypes>
// requires (std::derived_from<ResourceTypes, ResourceBase<ResourceTypes>> && ...)
class ResourceCompositionFactory : public ResourceFactory<ResourceBaseComposition<ResourceTypes...>,
                                                          ResourceComposition<ResourceTypes...>>,
                                   private Composition<ResourceFactory<ResourceTypes>...> {
    public:
        ResourceCompositionFactory() = default;

        ResourceCompositionFactory(
            std::unique_ptr<ExtensionFunction<ResourceBaseComposition<ResourceTypes...>>>
                extension_function,
            std::unique_ptr<FeasibilityFunction<ResourceBaseComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceBaseComposition<ResourceTypes...>>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceBaseComposition<ResourceTypes...>>>
                dominance_function)
            : ResourceFactory<ResourceBaseComposition<ResourceTypes...>>(
                  std::move(extension_function), std::move(feasibility_function),
                  std::move(cost_function), std::move(dominance_function)) {}

        virtual ~ResourceCompositionFactory() = default;

        template <typename... TypeTuples>
        std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource_base(
            const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            // Function to create each single resource component and set its value
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

            auto new_resource_composition = this->make_resource();
            this->apply(new_resource_composition,
                        [&](auto&& args_res_fac_vec, auto&& args_res_comp_vec) {
                            std::apply(
                                [&](auto&&... args_res_init_vec) {
                                    (make_resource_function(args_res_init_vec,
                                                            args_res_comp_vec,
                                                            args_res_fac_vec),
                                     ...);
                                },
                                resource_initializer);
                        });

            return new_resource_composition;
        }

        template <typename GraphResourceType>
        std::unique_ptr<ExtenderComposition<ResourceTypes...>> make_extender(
            const ResourceComposition<ResourceTypes...>& resource_base,
            const Arc<GraphResourceType>& arc) {
            const auto& resource_base_components = resource_base.get_type_components();

            auto new_extender_resource_composition =
                ResourceFactory<ExtenderComposition<ResourceTypes...>,
                                ResourceBaseComposition<ResourceTypes...>>::make_extender(arc);

            auto make_extender_function =
                [&](const auto& res_base_vec, const auto& res_fac_vec, auto& res_comp_vec) {
                    for (int i = 0; i < res_base_vec.size(); i++) {
                        const auto& res_base = *res_base_vec[i];
                        const auto& res_fac = res_fac_vec[i];
                        res_comp_vec.emplace_back(res_fac->make_extender(res_base, arc));
                    }
                };

            this->apply(new_extender_resource_composition,
                        [&](auto&& args_res_fac_vec, auto&& args_ext_comp_vec) {
                            std::apply(
                                [&](auto&&... args_res_base_vec) {
                                    (make_extender_function(args_res_base_vec,
                                                            args_res_fac_vec,
                                                            args_ext_comp_vec),
                                     ...);
                                },
                                resource_base_components);
                        });

            return new_extender_resource_composition;
        }

        // Add (move) the resource factory in argument to the right vector of resource factories
        // (i.e., ResourceTypeIndex).
        template <size_t ResourceTypeIndex, typename ResourceType>
        ResourceFactory<ResourceType>& add_resource_factory(
            std::unique_ptr<ResourceFactory<ResourceType>> resource_factory) {
            const auto& resource_factory_ref =
                this->template get_components<ResourceTypeIndex>().emplace_back(
                    std::move(resource_factory));
            update_resource_prototype();
            return *resource_factory_ref;
        }

        template <typename... TypeTuples>
        void update_extender(ExtenderComposition<ResourceTypes...>* extender_resource_composition,
                             const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            extender_resource_composition->for_each_component(
                resource_initializer,
                [&](auto&& res_comp, auto&& res_init) {
                    auto res_init_index = std::make_index_sequence<
                        std::tuple_size_v<typename std::remove_reference_t<decltype(res_init)>>>{};
                    set_value_single_resource(res_comp, res_init, res_init_index);
                });
        }

        template <typename TypeTuple, size_t ResourceTypeIndex>
        void update_extender(ExtenderComposition<ResourceTypes...>* extender_composition,
                             std::size_t resource_index,
                             const TypeTuple& single_resource_initializer) {
            const auto& res_comp =
                extender_composition->get_component<ResourceTypeIndex>(resource_index);
            auto res_init_index = std::make_index_sequence<std::tuple_size_v<
                typename std::remove_reference_t<decltype(single_resource_initializer)>>>{};
            set_value_single_resource(res_comp, single_resource_initializer, res_init_index);
        }

    private:
        const Resource<ResourceComposition<ResourceTypes...>>& update_resource_prototype() {
            auto update_prototype_function = [&](const auto& res_fac_vec, auto& prot_res_comp_vec) {
                prot_res_comp_vec.clear();
                for (int i = 0; i < res_fac_vec.size(); i++) {
                    const auto& res_fac = res_fac_vec[i];
                    prot_res_comp_vec.emplace_back(res_fac->make_resource());
                }
            };

            this->apply(this->resource_prototype_,
                        [&](auto&& args_res_fac_vec, auto&& args_prot_res_comp_vec) {
                            update_prototype_function(args_res_fac_vec, args_prot_res_comp_vec);
                        });

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
