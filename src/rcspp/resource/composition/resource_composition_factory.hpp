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
class ResourceCompositionFactory
    : public ResourceFactory<ResourceBaseComposition<ResourceTypes...>>,
      public Composition<ResourceFactory, ResourceTypes...> {
        using Base = ResourceFactory<ResourceBaseComposition<ResourceTypes...>>;
        using ResourceClass = Resource<ResourceBaseComposition<ResourceTypes...>>;
        using ExtenderClass = Extender<ResourceBaseComposition<ResourceTypes...>>;

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

        ~ResourceCompositionFactory() override = default;

        auto make_resource(size_t node_id) -> std::unique_ptr<ResourceClass> override {
            return Base::make_resource(node_id);
        }

        template <typename... TypeTuples>
        std::unique_ptr<ResourceClass> make_resource(
            const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            auto make_resource_function = [&](auto&& res_comp_vec,
                                              const auto& res_fac_vec,
                                              const auto& res_init_vec) {
                for (int i = 0; i < res_init_vec.size(); i++) {
                    res_comp_vec.emplace_back(res_fac_vec.at(i)->make_resource(res_init_vec[i]));
                }
            };

            auto new_resource_composition = this->make_resource();
            new_resource_composition->apply(*this, resource_initializer, make_resource_function);

            return new_resource_composition;
        }

        template <typename GraphResourceType>
        std::unique_ptr<ExtenderClass> make_extender(
            const std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            const Arc<GraphResourceType>& arc) {
            auto make_extender_function =
                [&](auto& ext_comp_vec, const auto& res_fac_vec, const auto& res_cons_vec) {
                    for (int i = 0; i < res_fac_vec.size(); i++) {
                        const auto& res_fac = res_fac_vec[i];
                        const auto& res_cons = res_cons_vec[i];
                        ext_comp_vec.emplace_back(res_fac->make_extender(res_cons, arc));
                    }
                };

            auto extender_resource_composition = Base::make_extender(arc);
            // Qualify to use the Extender's Composition<Extender,...> base (not Resource's).
            static_cast<Composition<Extender, ResourceTypes...>&>(*extender_resource_composition)
                .apply(*this, resource_consumption, make_extender_function);

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
        void update_extender(ExtenderClass* extender_composition,
                             const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            static_cast<Composition<Extender, ResourceTypes...>&>(*extender_composition)
                .for_each_component(
                    resource_initializer,
                    [](auto&& ext_comp, const auto& res_init) {
                        std::apply(
                            [&ext_comp](auto&&... args) {
                                ext_comp.set_value(std::forward<decltype(args)>(args)...);
                            },
                            res_init);
                    });
        }

        template <typename TypeTuple, size_t ResourceTypeIndex>
        void update_extender(ExtenderClass* extender_composition, std::size_t resource_index,
                             const TypeTuple& single_resource_initializer) {
            auto& res_comp =
                static_cast<Composition<Extender, ResourceTypes...>&>(*extender_composition)
                    .template get_component<ResourceTypeIndex>(resource_index);
            std::apply(
                [&res_comp](auto&&... args) {
                    res_comp.set_value(std::forward<decltype(args)>(args)...);
                },
                single_resource_initializer);
        }

    private:
        const ResourceClass& update_resource_prototype() {
            this->apply(*(this->resource_prototype_),
                        [&](const auto& res_fac_vec, auto& prot_res_comp_vec) {
                            prot_res_comp_vec.clear();
                            for (int i = 0; i < res_fac_vec.size(); i++) {
                                const auto& res_fac = res_fac_vec[i];
                                prot_res_comp_vec.emplace_back(res_fac->make_resource());
                            }
                        });

            return *(this->resource_prototype_);
        }
};
}  // namespace rcspp
