// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_algorithm_iterators.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/composition/functions/cost/component_cost_function.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/concrete/real_resource_factory.hpp"
#include "rcspp/resource/resource_traits.hpp"

template <typename... ResourceTypes>
class ResourceGraph : public Graph<ResourceComposition<ResourceTypes...>> {
    public:
        ResourceGraph(
            std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>>
                expansion_function,
            std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>>
                dominance_function)
            : resource_factory_(ResourceCompositionFactory<ResourceTypes...>(
                  std::move(expansion_function), std::move(feasibility_function),
                  std::move(cost_function), std::move(dominance_function))) {}

        ResourceGraph()
            : resource_factory_(ResourceCompositionFactory<ResourceTypes...>(
                  std::make_unique<CompositionExpansionFunction<RealResource>>(),
                  std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
                  std::make_unique<ComponentCostFunction<0, RealResource>>(0),
                  std::make_unique<CompositionDominanceFunction<RealResource>>())) {}

        template <typename ResourceType>
        void add_resource(std::unique_ptr<ExpansionFunction<ResourceType>> expansion_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::unique_ptr<DominanceFunction<ResourceType>> dominance_function) {
            constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
            using ResourceFactoryType = ResourceFactoryType_t<ResourceType>;

            resource_factory_.template add_resource_factory<ResourceTypeIndex, ResourceType>(
                std::make_unique<ResourceFactoryType>(std::move(expansion_function),
                                                      std::move(feasibility_function),
                                                      std::move(cost_function),
                                                      std::move(dominance_function)));
        }

        Node<ResourceComposition<ResourceTypes...>>& add_node(size_t node_id, bool source = false,
                                                              bool sink = false) override {
            auto& node =
                Graph<ResourceComposition<ResourceTypes...>>::add_node(node_id, source, sink);
            node.resource = std::move(resource_factory_.make_resource(node.id));

            return node;
        }

        Arc<ResourceComposition<ResourceTypes...>>& add_arc(
            const std::tuple<std::vector<ResourceInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            size_t origin_node_id, size_t destination_node_id,
            std::optional<size_t> arc_id = std::nullopt, double cost = 0.0,
            std::vector<Row> dual_rows = {}) {
            auto& arc = Graph<ResourceComposition<ResourceTypes...>>::add_arc(origin_node_id,
                                                                              destination_node_id,
                                                                              arc_id,
                                                                              cost,
                                                                              dual_rows);

            auto resource_base =
                resource_factory_
                    .template make_resource_base<ResourceInitializerTypeTuple_t<ResourceTypes>...>(
                        resource_consumption);

            auto expander = resource_factory_.make_expander(*resource_base, arc.id);

            arc.expander = std::move(expander);

            return arc;
        }

        template <typename... ExpanderResourceTypes>
        Arc<ResourceComposition<ResourceTypes...>>& add_arc(
            const std::tuple<ResourceInitializerTypeTuple_t<ExpanderResourceTypes>...>&
                expander_resource_consumption,
            size_t origin_node_id, size_t destination_node_id,
            std::optional<size_t> arc_id = std::nullopt, double cost = 0.0,
            std::vector<Row> dual_rows = {}) {
            std::tuple<std::vector<ResourceInitializerTypeTuple_t<ResourceTypes>>...>
                resource_consumption;

            auto apply_indices = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (([&] {
                     using ExpanderType =
                         std::tuple_element_t<Is, std::tuple<ExpanderResourceTypes...>>;
                     constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ExpanderType>;
                     auto& res_vec = std::get<ResourceTypeIndex>(resource_consumption);
                     const auto& res_cons = std::get<Is>(expander_resource_consumption);
                     res_vec.push_back(res_cons);  // push a single resource consumption
                 }()),
                 ...);
            };  // NOLINT

            apply_indices(std::make_index_sequence<sizeof...(ExpanderResourceTypes)>{});

            auto& arc = Graph<ResourceComposition<ResourceTypes...>>::add_arc(origin_node_id,
                                                                              destination_node_id,
                                                                              arc_id,
                                                                              cost,
                                                                              dual_rows);
            auto resource_base =
                resource_factory_
                    .template make_resource_base<ResourceInitializerTypeTuple_t<ResourceTypes>...>(
                        resource_consumption);
            auto expander = resource_factory_.make_expander(*resource_base, arc.id);
            arc.expander = std::move(expander);

            return arc;
        }

        ResourceCompositionFactory<ResourceTypes...>& get_resource_factory() {
            return resource_factory_;
        }

        void update_arc(
            Arc<ResourceComposition<ResourceTypes...>>* arc,
            const std::tuple<std::vector<ResourceInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            std::optional<double> cost = std::nullopt) {
            resource_factory_.update_expander(arc->expander.get(), resource_consumption);

            if (cost.has_value()) {
                arc->cost = cost.value();
            }
        }

        template <typename ResourceType>
        void update_arc(
            Arc<ResourceComposition<ResourceTypes...>>* arc, std::size_t resource_index,
            const ResourceInitializerTypeTuple_t<ResourceType>& single_resource_consumption,
            std::optional<double> cost = std::nullopt) {
            constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;

            resource_factory_.template update_expander<ResourceInitializerTypeTuple_t<ResourceType>,
                                                       ResourceTypeIndex>(
                arc->expander.get(),
                resource_index,
                single_resource_consumption);

            if (cost.has_value()) {
                arc->cost = cost.value();
            }
        }

        template <template <typename> class AlgorithmType = DominanceAlgorithmIterators>
        std::vector<Solution> solve() {
            AlgorithmType<ResourceComposition<ResourceTypes...>> algorithm(&resource_factory_,
                                                                           *this);
            return algorithm.solve();
        }

        template <typename CostResourceType = RealResource>
        void update_reduced_costs(const std::vector<double>& duals, size_t cost_index = 0) {
            for (auto& [arc_id, arc_ptr] : this->get_arcs_by_id()) {
                double reduced_cost = arc_ptr->cost;
                for (const auto& dual_row : arc_ptr->dual_rows) {
                    const auto dual_value = duals.at(dual_row.index);

                    reduced_cost -= dual_row.coefficient * dual_value;
                }

                update_arc<CostResourceType>(arc_ptr.get(), cost_index, reduced_cost);
            }
        }

    private:
        ResourceCompositionFactory<ResourceTypes...> resource_factory_;
};
