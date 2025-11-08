// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/algorithm/feasibility_preprocessor.hpp"
#include "rcspp/algorithm/shortest_path_preprocessor.hpp"
#include "rcspp/algorithm/shortest_path_sort.hpp"
#include "rcspp/algorithm/simple_dominance_algorithm_iterators.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/composition/functions/cost/component_cost_function.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class ResourceGraph : public Graph<ResourceComposition<ResourceTypes...>> {
    public:
        ResourceGraph(
            std::unique_ptr<ExpansionFunction<ResourceComposition<ResourceTypes...>>>
                extension_function,
            std::unique_ptr<FeasibilityFunction<ResourceComposition<ResourceTypes...>>>
                feasibility_function,
            std::unique_ptr<CostFunction<ResourceComposition<ResourceTypes...>>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceComposition<ResourceTypes...>>>
                dominance_function)
            : resource_factory_(ResourceCompositionFactory<ResourceTypes...>(
                  std::move(extension_function), std::move(feasibility_function),
                  std::move(cost_function), std::move(dominance_function))) {}

        ResourceGraph()
            : resource_factory_(ResourceCompositionFactory<ResourceTypes...>(
                  std::make_unique<CompositionExpansionFunction<RealResource>>(),
                  std::make_unique<CompositionFeasibilityFunction<RealResource>>(),
                  std::make_unique<ComponentCostFunction<0, RealResource>>(0),
                  std::make_unique<CompositionDominanceFunction<RealResource>>())) {}

        template <typename ResourceType>
        void add_resource(std::unique_ptr<ExpansionFunction<ResourceType>> extension_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::unique_ptr<DominanceFunction<ResourceType>> dominance_function) {
            constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ResourceType>;
            using ResourceFactoryType = ResourceFactory<ResourceType>;

            resource_factory_.template add_resource_factory<ResourceTypeIndex, ResourceType>(
                std::make_unique<ResourceFactoryType>(std::move(extension_function),
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

            auto extender = resource_factory_.make_extender(*resource_base, arc.id);

            arc.extender = std::move(extender);

            return arc;
        }

        template <typename... ExtenderResourceTypes>
        Arc<ResourceComposition<ResourceTypes...>>& add_arc(
            const std::tuple<ResourceInitializerTypeTuple_t<ExtenderResourceTypes>...>&
                extender_resource_consumption,
            size_t origin_node_id, size_t destination_node_id,
            std::optional<size_t> arc_id = std::nullopt, double cost = 0.0,
            std::vector<Row> dual_rows = {}) {
            std::tuple<std::vector<ResourceInitializerTypeTuple_t<ResourceTypes>>...>
                resource_consumption;

            auto apply_indices = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (([&] {
                     using ExtenderType =
                         std::tuple_element_t<Is, std::tuple<ExtenderResourceTypes...>>;
                     constexpr size_t ResourceTypeIndex = ResourceTypeIndex_v<ExtenderType>;
                     auto& res_vec = std::get<ResourceTypeIndex>(resource_consumption);
                     const auto& res_cons = std::get<Is>(extender_resource_consumption);
                     res_vec.push_back(res_cons);  // push a single resource consumption
                 }()),
                 ...);
            };  // NOLINT

            apply_indices(std::make_index_sequence<sizeof...(ExtenderResourceTypes)>{});

            auto& arc = Graph<ResourceComposition<ResourceTypes...>>::add_arc(origin_node_id,
                                                                              destination_node_id,
                                                                              arc_id,
                                                                              cost,
                                                                              dual_rows);
            auto resource_base =
                resource_factory_
                    .template make_resource_base<ResourceInitializerTypeTuple_t<ResourceTypes>...>(
                        resource_consumption);
            auto extender = resource_factory_.make_extender(*resource_base, arc.id);
            arc.extender = std::move(extender);

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
            resource_factory_.update_extender(arc->extender.get(), resource_consumption);

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

            resource_factory_.template update_extender<ResourceInitializerTypeTuple_t<ResourceType>,
                                                       ResourceTypeIndex>(
                arc->extender.get(),
                resource_index,
                single_resource_consumption);

            if (cost.has_value()) {
                arc->cost = cost.value();
            }
        }

        template <template <typename, typename...> class SortType = ShortestPathSort,
                  typename CostResourceType = RealResource>
        void sort_nodes_by_cost(size_t cost_index = 0) {
            SortType<CostResourceType, ResourceTypes...> sort(this, cost_index);
        }

        template <template <typename> class AlgorithmType = SimpleDominanceAlgorithmIterators,
                  typename CostResourceType = RealResource>
        std::vector<Solution> solve(double upper_bound = std::numeric_limits<double>::infinity(),
                                    bool preprocess = true, size_t cost_index = 0) {
            std::vector<std::unique_ptr<Preprocessor<ResourceComposition<ResourceTypes...>>>>
                preprocessors;
            if (preprocess) {
                // if first solve, try to remove some arcs based on feasibility
                if (!fesibility_processed_) {
                    process_feasibility();
                }

                // remove some arcs before solving the problem
                // the deleted arcs will be restored after the solve
                auto preprocessor =
                    std::make_unique<ShortestPathPreprocessor<CostResourceType, ResourceTypes...>>(
                        this,
                        upper_bound,
                        cost_index);
                preprocessor->preprocess();
                preprocessors.emplace_back(std::move(preprocessor));
            }

            // if not sorted, use default sort (by id)
            if (!this->are_nodes_sorted()) {
                this->sort_nodes();
            }

            // solve the rcspp
            AlgorithmType<ResourceComposition<ResourceTypes...>> algorithm(&resource_factory_,
                                                                           *this);
            std::vector<Solution> sols = algorithm.solve();

            // restore the removed arcs for the next resolution
            if (preprocess) {
                for (auto& preprocessor : preprocessors) {
                    preprocessor->restore();
                }
            }

            return sols;
        }

        void process_feasibility() {
            FeasibilityPreprocessor<ResourceComposition<ResourceTypes...>> feasibility_preprocessor(
                &resource_factory_,
                this);
            feasibility_preprocessor.preprocess();
            fesibility_processed_ = true;
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
        bool fesibility_processed_ = false;
};
}  // namespace rcspp
