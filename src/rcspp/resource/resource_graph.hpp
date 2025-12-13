// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>  // NOLINT
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/algorithm/simple_dominance_algorithm.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/preprocessor/connectivity_matrix.hpp"
#include "rcspp/preprocessor/feasibility_preprocessor.hpp"
#include "rcspp/preprocessor/shortest_path_connectivity_sort.hpp"
#include "rcspp/preprocessor/shortest_path_preprocessor.hpp"
#include "rcspp/resource/composition/functions/cost/component_cost_function.hpp"
#include "rcspp/resource/composition/resource_base_composition.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace rcspp {

template <typename... ResourceTypes>
class ResourceGraph : public Graph<ResourceBaseComposition<ResourceTypes...>> {
        using ResourceCompositionType = ResourceBaseComposition<ResourceTypes...>;

    public:
        ResourceGraph(
            std::unique_ptr<ExtensionFunction<ResourceCompositionType>> extension_function,
            std::unique_ptr<FeasibilityFunction<ResourceCompositionType>> feasibility_function,
            std::unique_ptr<CostFunction<ResourceCompositionType>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceCompositionType>> dominance_function)
            : resource_factory_(ResourceCompositionFactory<ResourceTypes...>(
                  std::move(extension_function), std::move(feasibility_function),
                  std::move(cost_function), std::move(dominance_function))),
              connectivityMatrix_(this) {}

        ResourceGraph()
            : resource_factory_(ResourceCompositionFactory<ResourceTypes...>(
                  std::make_unique<CompositionExtensionFunction<ResourceTypes...>>(),
                  std::make_unique<CompositionFeasibilityFunction<ResourceTypes...>>(),
                  std::make_unique<ComponentCostFunction<0, ResourceTypes...>>(0),
                  std::make_unique<CompositionDominanceFunction<ResourceTypes...>>())),
              connectivityMatrix_(this) {}

        ResourceGraph(const ResourceGraph&) = delete;
        ResourceGraph& operator=(const ResourceGraph&) = delete;
        ResourceGraph(ResourceGraph&&) = delete;
        ResourceGraph& operator=(ResourceGraph&&) = delete;

        virtual ~ResourceGraph() = default;

        template <typename ResourceType>
        void add_resource(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::unique_ptr<DominanceFunction<ResourceType>> dominance_function) {
            constexpr size_t ResourceTypeIndex =
                ComponentTypeIndex_v<ResourceType, ResourceTypes...>;
            using ResourceFactoryType = ResourceFactory<ResourceType>;

            resource_factory_.template add_resource_factory<ResourceTypeIndex, ResourceType>(
                std::make_unique<ResourceFactoryType>(std::move(extension_function),
                                                      std::move(feasibility_function),
                                                      std::move(cost_function),
                                                      std::move(dominance_function)));
        }

        Node<ResourceCompositionType>& add_node(size_t node_id, bool source = false,
                                                bool sink = false) override {
            auto& node = Graph<ResourceCompositionType>::add_node(node_id, source, sink);
            node.resource = resource_factory_.make_resource(node.id);

            return node;
        }

        Arc<ResourceCompositionType>& add_arc(
            const std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            size_t origin_node_id, size_t destination_node_id,
            std::optional<size_t> arc_id = std::nullopt, double cost = 0.0,
            std::vector<Row> dual_rows = {}) {
            auto& arc = Graph<ResourceCompositionType>::add_arc(origin_node_id,
                                                                destination_node_id,
                                                                arc_id,
                                                                cost,
                                                                dual_rows);

            auto extender = resource_factory_.make_extender(resource_consumption, arc);
            arc.extender = std::move(extender);
            return arc;
        }

        template <typename... ExtenderResourceTypes>
        Arc<ResourceCompositionType>& add_arc(
            const std::tuple<ComponentInitializerTypeTuple_t<ExtenderResourceTypes>...>&
                extender_resource_consumption,
            size_t origin_node_id, size_t destination_node_id,
            std::optional<size_t> arc_id = std::nullopt, double cost = 0.0,
            std::vector<Row> dual_rows = {}) {
            // build the full resource consumption tuple from the extender resource consumption
            std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>
                resource_consumption;
            auto apply_indices = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (([&] {
                     using ExtenderType =
                         std::tuple_element_t<Is, std::tuple<ExtenderResourceTypes...>>;
                     constexpr size_t ResourceTypeIndex =
                         ComponentTypeIndex_v<ExtenderType, ResourceTypes...>;
                     auto& res_vec = std::get<ResourceTypeIndex>(resource_consumption);
                     const auto& res_cons = std::get<Is>(extender_resource_consumption);
                     res_vec.push_back(res_cons);  // push a single resource consumption
                 }()),
                 ...);
            };  // NOLINT
            apply_indices(std::make_index_sequence<sizeof...(ExtenderResourceTypes)>{});

            return add_arc(resource_consumption,
                           origin_node_id,
                           destination_node_id,
                           arc_id,
                           cost,
                           dual_rows);
        }

        ResourceCompositionFactory<ResourceTypes...>& get_resource_factory() {
            return resource_factory_;
        }

        void update_arc(
            Arc<ResourceCompositionType>* arc,
            const std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            std::optional<double> cost = std::nullopt) {
            resource_factory_.update_extender(arc->extender.get(), resource_consumption);

            if (cost.has_value()) {
                arc->cost = cost.value();
            }
        }

        template <typename ResourceType>
        void update_arc(
            Arc<ResourceCompositionType>* arc, std::size_t resource_index,
            const ComponentInitializerTypeTuple_t<ResourceType>& single_resource_consumption,
            std::optional<double> cost = std::nullopt) {
            constexpr size_t ResourceTypeIndex =
                ComponentTypeIndex_v<ResourceType, ResourceTypes...>;

            resource_factory_
                .template update_extender<ComponentInitializerTypeTuple_t<ResourceType>,
                                          ResourceTypeIndex>(arc->extender.get(),
                                                             resource_index,
                                                             single_resource_consumption);

            if (cost.has_value()) {
                arc->cost = cost.value();
            }
        }

        // sort nodes by connectivity, break cycles on cost
        template <template <typename, typename...> class SortType = ShortestPathConnectivitySort,
                  typename CostResourceType = RealResource>
        void sort_nodes_by_connectivity(std::optional<size_t> cost_index = std::nullopt) {
            SortType<CostResourceType, ResourceTypes...> sort(this,
                                                              &connectivityMatrix_,
                                                              cost_index);
        }

        template <template <typename> class AlgorithmType, typename... Args>
        std::unique_ptr<AlgorithmType<ResourceCompositionType>> create_algorithm(Args&&... args) {
            return std::make_unique<AlgorithmType<ResourceCompositionType>>(
                &resource_factory_,
                std::forward<Args>(args)...);
        }

        template <template <typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename CostResourceType = RealResource>
        std::vector<Solution> solve(double upper_bound = std::numeric_limits<double>::infinity(),
                                    AlgorithmParams params = {}, bool preprocess = true,
                                    int cost_index = 0) {
            AlgorithmType<ResourceCompositionType> algorithm(&resource_factory_, params);
            return solve(&algorithm, upper_bound, preprocess, cost_index);
        }

        template <typename CostResourceType = RealResource, template <typename> class AlgorithmType>
        std::vector<Solution> solve(AlgorithmType<ResourceCompositionType>* algorithm,
                                    double upper_bound = std::numeric_limits<double>::infinity(),
                                    bool preprocess = true, int cost_index = 0) {
            if (this->get_source_node_ids().empty() || this->get_sink_node_ids().empty()) {
                LOG_WARN("ResourceGraph::solve: No source or sink nodes defined in the graph.");
                return {};
            }

            // try to acquire the mutex without blocking
            std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
            if (!lock.owns_lock()) {
                LOG_WARN(
                    "ResourceGraph::solve: Cannot lock the mutex. Concurrent solves are not "
                    "allowed.");
                return {};
            }

            std::vector<std::unique_ptr<Preprocessor<ResourceCompositionType>>> preprocessors;
            if (preprocess) {
                // if graph has been modified, try to remove some arcs based on feasibility
                // initialize or update connectivity matrix
                if (this->is_modified()) {
                    process_feasibility();
                    connectivityMatrix_.compute_bitmatrix();
                }

                // if not sorted, use default sort by connectivity
                if (!this->are_nodes_sorted()) {
                    this->sort_nodes_by_connectivity();
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
            std::vector<Solution> sols = algorithm->solve(this, upper_bound);

            // restore the removed arcs for the next resolution
            if (preprocess) {
                for (auto& preprocessor : preprocessors) {
                    preprocessor->restore();
                }
                this->track_modifications();  // mark as unmodified after restoring arcs
            }

            return sols;
        }

        void process_feasibility() {
            FeasibilityPreprocessor<ResourceCompositionType> feasibility_preprocessor(
                &resource_factory_,
                this);
            feasibility_preprocessor.preprocess();
        }

        bool is_connected(size_t origin_node_id, size_t destination_node_id) {
            if (this->is_modified()) {
                connectivityMatrix_.compute_bitmatrix();
                this->track_modifications();
            }

            return connectivityMatrix_.is_connected(origin_node_id, destination_node_id);
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
        ConnectivityMatrix<ResourceCompositionType> connectivityMatrix_;
        std::mutex mutex_;
};
}  // namespace rcspp
