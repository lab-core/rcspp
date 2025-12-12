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
                                   public Composition<ResourceFactory<ResourceTypes>...> {
        using Base = ResourceFactory<ResourceBaseComposition<ResourceTypes...>,
                                     ResourceComposition<ResourceTypes...>>;

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
            : Base(std::move(extension_function), std::move(feasibility_function),
                   std::move(cost_function), std::move(dominance_function)) {}

        virtual ~ResourceCompositionFactory() = default;

        template <typename... TypeTuples>
        std::unique_ptr<ResourceComposition<ResourceTypes...>> make_resource_base(
            const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            // Function to create each single resource component and set its value
            auto make_resource_function = [&](auto&& res_comp_vec,
                                              const auto& res_fac_vec,
                                              const auto& res_init_vec) {
                for (int i = 0; i < res_init_vec.size(); i++) {
                    const auto& res_init = res_init_vec[i];

                    auto& res_comp =
                        res_comp_vec.emplace_back(res_fac_vec.at(i)->make_resource_base());

                    auto res_init_index = std::make_index_sequence<
                        std::tuple_size_v<typename std::remove_reference_t<decltype(res_init)>>>{};

                    set_value_single_resource(res_comp.get(), res_init, res_init_index);
                }
            };

            auto new_resource_composition = this->make_resource();
            new_resource_composition->apply(*this, resource_initializer, make_resource_function);

            return new_resource_composition;
        }

        template <typename GraphResourceType>
        std::unique_ptr<ExtenderComposition<ResourceTypes...>> make_extender(
            const ResourceComposition<ResourceTypes...>& resource_base,
            const Arc<GraphResourceType>& arc) {
            auto make_extender_function =
                [&](auto& ext_comp_vec, const auto& res_base_vec, const auto& res_fac_vec) {
                    for (int i = 0; i < res_base_vec.size(); i++) {
                        const auto& res_base = *res_base_vec[i];
                        const auto& res_fac = res_fac_vec[i];
                        ext_comp_vec.emplace_back(res_fac->make_extender(res_base, arc));
                    }
                };

            auto extender_resource_composition =
                Base::template make_extender<ExtenderComposition<ResourceTypes...>>(arc);
            extender_resource_composition->apply(resource_base, *this, make_extender_function);

            return extender_resource_composition;
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
        void update_extender(Extender<ResourceBaseComposition<ResourceTypes...>>* extender,
                             const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            auto* extender_composition =
                static_cast<ExtenderComposition<ResourceTypes...>*>(extender);
            extender_composition->for_each_component(
                resource_initializer,
                [&](auto&& res_comp, auto&& res_init) {
                    auto res_init_index = std::make_index_sequence<
                        std::tuple_size_v<typename std::remove_reference_t<decltype(res_init)>>>{};
                    set_value_single_resource(res_comp.get(), res_init, res_init_index);
                });
        }

        template <typename TypeTuple, size_t ResourceTypeIndex>
        void update_extender(Extender<ResourceBaseComposition<ResourceTypes...>>* extender,
                             std::size_t resource_index,
                             const TypeTuple& single_resource_initializer) {
            auto* extender_composition =
                static_cast<ExtenderComposition<ResourceTypes...>*>(extender);
            auto& res_comp =
                extender_composition->template get_component<ResourceTypeIndex>(resource_index);
            auto res_init_index = std::make_index_sequence<std::tuple_size_v<
                typename std::remove_reference_t<decltype(single_resource_initializer)>>>{};
            set_value_single_resource(&res_comp, single_resource_initializer, res_init_index);
        }

    private:
        const ResourceComposition<ResourceTypes...>& update_resource_prototype() {
            this->apply(this->resource_prototype_,
                        [&](const auto& res_fac_vec, auto& prot_res_comp_vec) {
                            prot_res_comp_vec.clear();
                            for (int i = 0; i < res_fac_vec.size(); i++) {
                                const auto& res_fac = res_fac_vec[i];
                                prot_res_comp_vec.emplace_back(res_fac->make_resource());
                            }
                        });

            return *(this->resource_prototype_);
        }

        template <typename ResourceType, typename TypeTuple, std::size_t... N>
        auto set_value_single_resource(ResourceType* resource,
                                       const TypeTuple& resource_initializer,
                                       std::index_sequence<N...> /*unused*/) {
            resource->set_value(std::get<N>(resource_initializer)...);
        }
};
}  // namespace rcspp
