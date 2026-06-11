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

/// @brief Factory that creates and manages resources for a composed resource type.
///
/// `ResourceCompositionFactory` extends `ResourceFactory` for a `ResourceTypeComposition` and
/// stores one `ResourceFactory<RT>` per constituent type @p RT via the `Composition` mixin.
/// It coordinates the construction and update of composed resources and their extenders.
///
/// @tparam ResourceTypes The individual resource types that form the composition.
template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ResourceCompositionFactory
    : public ResourceFactory<ResourceTypeComposition<ResourceTypes...>>,
      public Composition<ResourceFactory, ResourceTypes...> {
        using Base = ResourceFactory<ResourceTypeComposition<ResourceTypes...>>;
        using ResourceClass = Resource<ResourceTypeComposition<ResourceTypes...>>;
        using ExtenderClass = Extender<ResourceTypeComposition<ResourceTypes...>>;

    public:
        /// @brief Default constructor — creates an empty factory with no sub-factories.
        ResourceCompositionFactory() = default;

        /// @brief Constructs the factory with composition-level function objects.
        ///
        /// @param extension_function   Owning pointer to the composition extension function.
        /// @param feasibility_function Owning pointer to the composition feasibility function.
        /// @param cost_function        Owning pointer to the composition cost function.
        /// @param dominance_function   Owning pointer to the composition dominance function.
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

        /// @brief Destructor.
        ~ResourceCompositionFactory() override = default;

        // The user-declared destructor suppresses the implicit move constructor/assignment.
        // Explicitly default them so ResourceGraph's private constructor can move-construct
        // resource_factory_.

        /// @brief Move constructor (explicitly defaulted to restore suppressed implicit move).
        ResourceCompositionFactory(ResourceCompositionFactory&&) = default;

        /// @brief Move assignment operator (explicitly defaulted).
        ResourceCompositionFactory& operator=(ResourceCompositionFactory&&) = default;

        /// @brief Creates a new composed resource for the given node using the stored prototype.
        ///
        /// @param node_id Identifier of the target node.
        /// @return Owning pointer to the newly created composed resource.
        auto create_resource(size_t node_id) -> std::unique_ptr<ResourceClass> override {
            return Base::create_resource(node_id);
        }

        /// @brief Creates a new composed resource for a node and initialises sub-resources.
        ///
        /// After creating the resource via `create_resource(node_id)`, each sub-resource
        /// component has its value set from the corresponding entry in @p resource_initializer.
        ///
        /// @tparam TypeTuples   Tuple-element types matching each constituent resource type.
        /// @param node_id             Identifier of the target node.
        /// @param resource_initializer Tuple of per-type vectors of initializer tuples.
        /// @return Owning pointer to the initialised composed resource.
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

        /// @brief Creates a composed resource by delegating to each per-type sub-factory.
        ///
        /// For each constituent resource type, iterates over the initializer vector and calls
        /// the corresponding sub-factory's `create_resource` to build sub-resources.
        ///
        /// @tparam TypeTuples   Tuple-element types matching each constituent resource type.
        /// @param resource_initializer Tuple of per-type vectors of initializer tuples.
        /// @return Owning pointer to the newly created composed resource.
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

        /// @brief Creates a composed extender for the given arc.
        ///
        /// Builds the top-level extender via `Base::create_extender(arc)` and then
        /// populates each per-type slot by delegating to the corresponding sub-factory's
        /// `create_extender`.
        ///
        /// @tparam GraphResourceType The resource type of the owning graph.
        /// @param resource_consumption Tuple of per-type vectors of consumption initializer tuples.
        /// @param arc                  The arc for which to create the extender.
        /// @return Owning pointer to the newly created composed extender.
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

            return extender_resource_composition;
        }

        /// @brief Adds a sub-factory to the composition and refreshes the resource prototype.
        ///
        /// Moves @p resource_factory into the per-type vector at @p ResourceTypeIndex, then
        /// calls `update_resource_prototype()` to keep the composition's prototype consistent.
        ///
        /// @tparam ResourceTypeIndex Zero-based index of the target type slot in the composition.
        /// @tparam ResourceType      The resource type managed by the sub-factory.
        /// @param resource_factory   Owning pointer to the sub-factory to add.
        /// @return Reference to the newly added `ResourceFactory<ResourceType>`.
        template <size_t ResourceTypeIndex, typename ResourceType>
        ResourceFactory<ResourceType>& add_resource_factory(
            std::unique_ptr<ResourceFactory<ResourceType>> resource_factory) {
            const auto& resource_factory_ref =
                this->template get_components<ResourceTypeIndex>().emplace_back(
                    std::move(resource_factory));
            update_resource_prototype();
            return *resource_factory_ref;
        }

        /// @brief Updates all sub-extenders in a composed extender from a full initializer tuple.
        ///
        /// For each constituent type slot, calls `set_value` on each sub-extender using the
        /// corresponding initializer tuple.
        ///
        /// @tparam TypeTuples         Tuple-element types matching each constituent resource type.
        /// @param extender_composition The composed extender to update.
        /// @param resource_initializer Tuple of per-type vectors of initializer tuples.
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

        /// @brief Updates a single sub-extender within a composed extender.
        ///
        /// Calls `set_value` on the sub-extender at @p resource_index in the type slot
        /// identified by @p ResourceTypeIndex.
        ///
        /// @tparam TypeTuple          The initializer tuple type for the targeted resource type.
        /// @tparam ResourceTypeIndex  Zero-based index of the target type slot.
        /// @param extender_composition        The composed extender to update.
        /// @param resource_index              Position of the sub-extender to update.
        /// @param single_resource_initializer The initializer tuple to apply.
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

        /// @brief Returns the number of sub-factories registered for a given resource type.
        ///
        /// @tparam ResourceType The resource type whose slot count is queried.
        /// @return Number of `ResourceFactory<ResourceType>` entries in the corresponding slot.
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
            return new_factory;
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
