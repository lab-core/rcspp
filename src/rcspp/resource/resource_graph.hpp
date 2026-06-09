// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>  // NOLINT
#include <stdexcept>
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
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace rcspp {

template <typename... ResourceTypes>
    requires(ResourceTypeConcept<ResourceTypes> && ...)
class ResourceGraph : public Graph<ResourceTypeComposition<ResourceTypes...>> {
        using ResourceCompositionType = ResourceTypeComposition<ResourceTypes...>;

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
            node.resource = resource_factory_.create_resource(node.id);

            return node;
        }

        template <typename ResourceType>
        void add_resource(
            std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
            std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
            std::unique_ptr<CostFunction<ResourceType>> cost_function,
            std::unique_ptr<DominanceFunction<ResourceType>> dominance_function,
            ComponentInitializerTypeTuple_t<ResourceType> default_resource_initializer) {
            constexpr size_t ResourceTypeIndex =
                ComponentTypeIndex_v<ResourceType, ResourceTypes...>;
            using ResourceFactoryType = ResourceFactory<ResourceType>;
            auto create_prototype = []<typename... Args>(Args&&... args) {
                return ResourceType(std::forward<Args>(args)...);
            };  // NOLINT
            ResourceType resource_base_prototype =
                std::apply(create_prototype, default_resource_initializer);
            resource_factory_.template add_resource_factory<ResourceTypeIndex, ResourceType>(
                std::make_unique<ResourceFactoryType>(std::move(extension_function),
                                                      std::move(feasibility_function),
                                                      std::move(cost_function),
                                                      std::move(dominance_function),
                                                      resource_base_prototype));
        }

        Node<ResourceCompositionType>& add_node(
            size_t node_id,
            const std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_initializer,
            bool source = false, bool sink = false) {
            auto& node = Graph<ResourceCompositionType>::add_node(node_id, source, sink);
            node.resource = resource_factory_.create_resource(node.id, resource_initializer);
            return node;
        }

        template <typename... ResourceInitTypes>
        Node<ResourceCompositionType>& add_node(
            size_t node_id,
            const std::tuple<ComponentInitializerTypeTuple_t<ResourceInitTypes>...>&
                resource_init_values,
            bool source = false, bool sink = false) {
            std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>
                resource_initializer;
            auto apply_indices = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (([&] {
                     using InitType = std::tuple_element_t<Is, std::tuple<ResourceInitTypes...>>;
                     constexpr size_t ResourceTypeIndex =
                         ComponentTypeIndex_v<InitType, ResourceTypes...>;
                     auto& res_vec = std::get<ResourceTypeIndex>(resource_initializer);
                     const auto& res_cons = std::get<Is>(resource_init_values);
                     res_vec.push_back(res_cons);
                 }()),
                 ...);
            };  // NOLINT
            apply_indices(std::make_index_sequence<sizeof...(ResourceInitTypes)>{});
            return add_node(node_id, resource_initializer, source, sink);
        }

        Arc<ResourceCompositionType>& add_arc(
            const std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>&
                resource_consumption,
            size_t origin_node_id, size_t destination_node_id, double cost = 0.0,
            std::vector<Row> rows = {}) {
            auto& arc = Graph<ResourceCompositionType>::add_arc(origin_node_id,
                                                                destination_node_id,
                                                                cost,
                                                                rows);

            auto extender = resource_factory_.create_extender(resource_consumption, arc);
            arc.extender = std::move(extender);
            return arc;
        }

        template <typename... ExtenderResourceTypes>
        Arc<ResourceCompositionType>& add_arc(
            const std::tuple<ComponentInitializerTypeTuple_t<ExtenderResourceTypes>...>&
                extender_resource_consumption,
            size_t origin_node_id, size_t destination_node_id, double cost = 0.0,
            std::vector<Row> rows = {}) {
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

            return add_arc(resource_consumption, origin_node_id, destination_node_id, cost, rows);
        }

        ResourceCompositionFactory<ResourceTypes...>& get_resource_factory() {
            return resource_factory_;
        }

        /// @brief Return a deep copy of this ResourceGraph with stable arc IDs.
        ///
        /// Clones the resource factory (so the new graph has independent resource
        /// prototypes and extension functions), then delegates topology copying to
        /// Graph::clone_topology_into(), which:
        ///  - creates nodes via ResourceGraph::add_node() (initialises resources from
        ///    the cloned factory) and overrides with clones of the original resources;
        ///  - copies arcs at their exact IDs using add_arc_at, then clones extenders.
        ///
        /// @param include_rows       Copy arc rows (set false for topology-only clones).
        /// @param clone_removed_arcs Also clone removed arcs (re-removed in the clone).
        [[nodiscard]] std::unique_ptr<ResourceGraph<ResourceTypes...>> clone(
            bool include_rows = true, bool clone_removed_arcs = false) const {
            auto factory_clone = resource_factory_.clone_factory();
            // Use raw new: clone() is a ResourceGraph member and can access the private
            // constructor; std::make_unique cannot (it's an external template).
            auto new_rg = std::unique_ptr<ResourceGraph<ResourceTypes...>>(
                new ResourceGraph<ResourceTypes...>(std::move(*factory_clone)));
            Graph<ResourceCompositionType>::clone_topology_into(*new_rg,
                                                                include_rows,
                                                                clone_removed_arcs);
            return new_rg;
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
            requires is_numerical_resource_v<CostResourceType>
        void sort_nodes_by_connectivity(std::optional<size_t> cost_index = std::nullopt) {
            SortType<CostResourceType, ResourceTypes...> sort(this,
                                                              &connectivityMatrix_,
                                                              cost_index);
        }

        template <template <typename, typename> class AlgorithmType,
                  typename LabelContainerType = LabelList<ResourceCompositionType>,
                  typename... Args>
        std::unique_ptr<AlgorithmType<ResourceCompositionType, LabelContainerType>>
        create_algorithm(Args&&... args) {
            return std::make_unique<AlgorithmType<ResourceCompositionType, LabelContainerType>>(
                &resource_factory_,
                std::forward<Args>(args)...);
        }

        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename CostResourceType = RealResource,
                  typename LabelContainerType = LabelList<ResourceCompositionType>>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(
            double upper_bound = std::numeric_limits<double>::infinity(),
            AlgorithmParams<LabelContainerType> params = AlgorithmParams<LabelContainerType>(),
            bool preprocess = true, size_t cost_index = 0) {
            AlgorithmType<ResourceCompositionType, LabelContainerType> algorithm(&resource_factory_,
                                                                                 params);
            return solve<AlgorithmType<ResourceCompositionType, LabelContainerType>,
                         CostResourceType>(&algorithm, upper_bound, preprocess, cost_index);
        }

        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename CostResourceType = RealResource,
                  typename LabelContainerType = LabelList<ResourceCompositionType>>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(AlgorithmParams<LabelContainerType> params, bool preprocess = true,
                          size_t cost_index = 0) {
            AlgorithmType<ResourceCompositionType, LabelContainerType> algorithm(&resource_factory_,
                                                                                 params);
            return solve<AlgorithmType<ResourceCompositionType, LabelContainerType>,
                         CostResourceType>(&algorithm,
                                           std::numeric_limits<double>::infinity(),
                                           preprocess,
                                           cost_index);
        }

        /// @brief Solve using base algorithm parameters (without explicit container type).
        ///
        /// Convenience overload that wraps @p base_params in a default-constructed
        /// AlgorithmParams so callers only need to set the base fields (e.g. memory
        /// limits, stop conditions) without knowing the internal ResourceCompositionType.
        ///
        /// @param base_params    Base algorithm parameters (memory limits, stop conditions…).
        /// @param upper_bound    Cost upper bound; solutions above this are discarded.
        /// @param preprocess     Whether to run preprocessing before solving.
        /// @param cost_index     Index of the cost component to use.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename CostResourceType = RealResource,
                  typename LabelContainerType = LabelList<ResourceCompositionType>>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(AlgorithmBaseParams base_params,
                          double upper_bound = std::numeric_limits<double>::infinity(),
                          bool preprocess = true, size_t cost_index = 0) {
            return solve<AlgorithmType, CostResourceType, LabelContainerType>(
                upper_bound,
                AlgorithmParams<LabelContainerType>(std::move(base_params)),
                preprocess,
                cost_index);
        }

        template <typename AlgorithmType, typename CostResourceType = RealResource>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(  // NOLINT(readability-function-cognitive-complexity)
            AlgorithmType* algorithm, double upper_bound = std::numeric_limits<double>::infinity(),
            bool preprocess = true, size_t cost_index = 0) {
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

                // shortest-path preprocessing requires a numerical cost resource in the pack.
                // Use ComponentTypeIndex<...>::value rather than the _v alias: the _v alias is
                // a constrained variable template that is undeclared when the type isn't in the
                // pack, which would make this condition ill-formed (even inside if constexpr).
                if constexpr (is_numerical_resource_v<CostResourceType> &&
                              ComponentTypeIndex<CostResourceType, ResourceTypes...>::value != -1) {
                    // check if the cost index is correct (size_t -> no negative case)
                    if (cost_index >=
                        resource_factory_.template get_num_resource_type<CostResourceType>()) {
                        // check if not the default value
                        if (cost_index > 0) {
                            LOG_WARN(
                                "ResourceGraph::solve: cost_index is out of bounds for the number "
                                "of extender components of the cost resource. ",
                                cost_index,
                                " for a length of ",
                                resource_factory_
                                    .template get_num_resource_type<CostResourceType>());
                        }
                    } else {
                        // if not sorted, use default sort by connectivity. Forward
                        // cost_index so the sort's Bellman-Ford distances read the same
                        // extender cost component the preprocessor (and the labeling
                        // algorithm) will use. Without this, the sort silently falls back
                        // to arc.cost and can disagree with the preprocessor whenever the
                        // chosen cost slot differs from the base arc cost -- e.g. after
                        // update_reduced_costs has rewritten extender slot cost_index.
                        if (!this->are_nodes_sorted()) {
                            this->template sort_nodes_by_connectivity<ShortestPathConnectivitySort,
                                                                      CostResourceType>(cost_index);
                        }

                        // remove some arcs before solving the problem
                        // the deleted arcs will be restored after the solve
                        auto preprocessor = std::make_unique<
                            ShortestPathPreprocessor<CostResourceType, ResourceTypes...>>(
                            this,
                            upper_bound,
                            cost_index);
                        preprocessor->preprocess();
                        preprocessors.emplace_back(std::move(preprocessor));
                    }
                }
            }

            // if not sorted, use default sort (by id)
            if (!this->are_nodes_sorted()) {
                this->sort_nodes();
            }

            // ensure CSR is up-to-date (preprocessing may have removed arcs after sort)
            // Also ensure that the arcs are sorted, use default sort (by id)
            this->build_csr();

            // solve the rcspp
            SolveResult result = algorithm->solve(this, upper_bound);

            // restore the removed arcs for the next resolution
            if (preprocess) {
                for (auto& preprocessor : preprocessors) {
                    preprocessor->restore();
                }
                this->track_modifications();  // mark as unmodified after restoring arcs
            }

            return result;
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

        // Constrained to RealResource on purpose. The body computes the reduced cost as a
        // double (LP duals are inherently fractional) and feeds it to
        // update_arc<CostResourceType>(...), which forwards into a tuple whose element
        // type is CostResourceType::ValueType. For an integral cost (e.g. IntResource) the
        // double -> int conversion would silently truncate, dropping fractional reduced
        // costs and -- critically for column generation -- collapsing reduced costs in
        // (-1, 0) to 0, hiding improving columns from pricing. For UIntResource the
        // negative-to-unsigned conversion is implementation-defined and usually wraps to
        // huge positive numbers, which is even worse. If integer-cost reduced-cost
        // updates ever become a real use case, add a separate function with an explicit
        // scaling/rounding policy rather than relaxing this constraint.
        template <typename CostResourceType = RealResource>
            requires std::is_same_v<CostResourceType, RealResource>
        void update_reduced_costs(const std::vector<double>& duals, size_t cost_index = 0) {
            // Bounds-check cost_index once, up front. Without this, an out-of-range
            // cost_index would throw std::out_of_range from inside the factory's
            // get_component<I>(index).at() call on the first arc processed, leaving
            // the graph in an inconsistent state (some arcs updated, others not).
            const auto cost_len =
                resource_factory_.template get_num_resource_type<CostResourceType>();
            if (cost_index >= cost_len) {
                LOG_WARN("ResourceGraph::update_reduced_costs: cost_index ",
                         cost_index,
                         " is out of bounds for the cost resource (",
                         cost_len,
                         " component(s)). No arcs updated.");
                return;
            }

            this->for_each_arc([&](auto& arc) {
                double reduced_cost = arc.cost;
                for (const auto& row : arc.rows) {
                    if (row.index >= duals.size()) {
                        throw std::out_of_range(
                            "ResourceGraph::update_reduced_costs: dual index " +
                            std::to_string(row.index) +
                            " is out of range (duals.size()=" + std::to_string(duals.size()) + ")");
                    }
                    reduced_cost -= row.coefficient * duals[row.index];
                }
                update_arc<CostResourceType>(&arc, cost_index, reduced_cost);
            });
        }

    private:
        /// @brief Construct directly from a pre-built factory (used by clone()).
        explicit ResourceGraph(ResourceCompositionFactory<ResourceTypes...>&& factory)
            : resource_factory_(std::move(factory)), connectivityMatrix_(this) {}

        ResourceCompositionFactory<ResourceTypes...> resource_factory_;
        ConnectivityMatrix<ResourceCompositionType> connectivityMatrix_;
        std::mutex mutex_;
};
}  // namespace rcspp
