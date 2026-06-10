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
#include "rcspp/resource/composition/resource_type_composition.hpp"

namespace rcspp {

template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ResourceCompositionFactory
    : public ResourceFactory<ResourceTypeComposition<ResourceTypes...>>,
      public Composition<ResourceFactory, ResourceTypes...> {
        using Base = ResourceFactory<ResourceTypeComposition<ResourceTypes...>>;
        using ResourceClass = Resource<ResourceTypeComposition<ResourceTypes...>>;
        using ExtenderClass = Extender<ResourceTypeComposition<ResourceTypes...>>;

    public:
        ResourceCompositionFactory() = default;

        ResourceCompositionFactory(
            std::unique_ptr<ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>>
                extension_function,
            std::unique_ptr<FeasibilityFunction<ResourceTypeComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceTypeComposition<ResourceTypes...>>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceTypeComposition<ResourceTypes...>>>
                dominance_function)
            : Base(std::move(extension_function), std::move(feasibility_function),
                   std::move(cost_function), std::move(dominance_function)) {}

        ~ResourceCompositionFactory() override = default;

        // The user-declared destructor suppresses the implicit move constructor/assignment.
        // Explicitly default them so ResourceGraph's private constructor can move-construct
        // resource_factory_.
        ResourceCompositionFactory(ResourceCompositionFactory&&) = default;
        ResourceCompositionFactory& operator=(ResourceCompositionFactory&&) = default;

        auto create_resource(size_t node_id) -> std::unique_ptr<ResourceClass> override {
            return Base::create_resource(node_id);
        }

        template <typename... TypeTuples>
        std::unique_ptr<Resource<ResourceTypeComposition<ResourceTypes...>>> create_resource(
            size_t node_id, const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            // Reset for node_id first (preprocess functions), then apply initializer values
            auto new_resource = create_resource(node_id);
            new_resource->for_each_component(
                resource_initializer,
                [](auto&& res_comp, const auto& res_init) {
                    std::apply(
                        [&res_comp](auto&&... args) {
                            res_comp.set_value(std::forward<decltype(args)>(args)...);
                        },
                        res_init);
                });
            return new_resource;
        }

        template <typename... TypeTuples>
        std::unique_ptr<ResourceClass> create_resource(
            const std::tuple<std::vector<TypeTuples>...>& resource_initializer) {
            auto create_resource_function = [&](auto&& res_comp_vec,
                                                const auto& res_fac_vec,
                                                const auto& res_init_vec) {
                for (size_t i = 0; i < res_init_vec.size(); i++) {
                    res_comp_vec.emplace_back(res_fac_vec.at(i)->create_resource(res_init_vec[i]));
                }
            };

            auto new_resource_composition = this->create_resource();
            new_resource_composition->apply(*this, resource_initializer, create_resource_function);

            return new_resource_composition;
        }

        template <typename GraphResourceType>
        std::unique_ptr<ExtenderClass> create_extender(
            const std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            const Arc<GraphResourceType>& arc) {
            auto create_extender_function =
                [&](auto& ext_comp_vec, const auto& res_fac_vec, const auto& res_cons_vec) {
                    for (size_t i = 0; i < res_fac_vec.size(); i++) {
                        const auto& res_fac = res_fac_vec[i];
                        const auto& res_cons = res_cons_vec[i];
                        ext_comp_vec.emplace_back(res_fac->create_extender(res_cons, arc));
                    }
                };

            auto extender_resource_composition = Base::create_extender(arc);
            extender_resource_composition->apply(*this,
                                                 resource_consumption,
                                                 create_extender_function);

            return extender_resource_composition;  // GCOVR_EXCL_LINE
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
            extender_composition->for_each_component(
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
                extender_composition->template get_component<ResourceTypeIndex>(resource_index);
            std::apply(
                [&res_comp](auto&&... args) {
                    res_comp.set_value(std::forward<decltype(args)>(args)...);
                },
                single_resource_initializer);
        }

        template <typename ResourceType>
        [[nodiscard]] size_t get_num_resource_type() const {
            constexpr size_t ResourceTypeIndex =
                ComponentTypeIndex_v<ResourceType, ResourceTypes...>;
            return this->template get_components<ResourceTypeIndex>().size();
        }

        /// @brief Return a deep copy of this factory.
        ///
        /// Clones each per-type ResourceFactory (via the Composition copy constructor,
        /// which calls clone() on every element) then rebuilds the composition-level
        /// resource prototype from the cloned sub-factories.
        [[nodiscard]] std::unique_ptr<ResourceCompositionFactory<ResourceTypes...>> clone_factory()
            const {
            // Construct with default composition functions so resource_prototype_ is
            // valid before update_resource_prototype() is called.
            auto new_factory = std::make_unique<ResourceCompositionFactory<ResourceTypes...>>(
                std::make_unique<CompositionExtensionFunction<ResourceTypes...>>(),
                std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
                std::make_unique<CompositionCostFunction<ResourceTypes...>>(),
                std::make_unique<CompositionDominanceFunction<ResourceTypes...>>());
            // Copy-assign the Composition<ResourceFactory, ResourceTypes...> part.
            // The assignment triggers the copy constructor, which calls clone() on
            // every ResourceFactory<RT> in the per-type vectors.
            static_cast<Composition<ResourceFactory, ResourceTypes...>&>(*new_factory) =
                static_cast<const Composition<ResourceFactory, ResourceTypes...>&>(*this);
            // Rebuild resource_prototype_ from the newly cloned sub-factories.
            new_factory->update_resource_prototype();
            return new_factory;  // GCOVR_EXCL_LINE
        }

    private:
        const ResourceClass& update_resource_prototype() {
            this->apply(*(this->resource_prototype_),
                        [&](const auto& res_fac_vec, auto& prot_res_comp_vec) {
                            prot_res_comp_vec.clear();
                            for (size_t i = 0; i < res_fac_vec.size(); i++) {
                                const auto& res_fac = res_fac_vec[i];
                                prot_res_comp_vec.emplace_back(res_fac->create_resource());
                            }
                        });

            return *(this->resource_prototype_);
        }
};
}  // namespace rcspp
