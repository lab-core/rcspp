// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <mutex>  // NOLINT
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/directional_implementations.hpp"
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
#include "rcspp/validation/pre_solve_plan.hpp"

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

        /// @brief Adds a resource: its extension, feasibility, cost and dominance functions.
        ///
        /// Backward coherence is not checked here, since forward-only models need not satisfy
        /// it; the model checks validate it before a backward or bidirectional solve, and callers
        /// may assert @c declares_directional_resource_kind_v themselves.
        ///
        /// @tparam ResourceType The component's resource type.
        /// @param extension_function   How an arc modifies the resource.
        /// @param feasibility_function Whether a value is within bounds.
        /// @param cost_function        The resource's contribution to the objective.
        /// @param dominance_function   Whether one value makes another redundant.
        template <typename ResourceType>
        void add_resource(std::unique_ptr<ExtensionFunction<ResourceType>> extension_function,
                          std::unique_ptr<FeasibilityFunction<ResourceType>> feasibility_function,
                          std::unique_ptr<CostFunction<ResourceType>> cost_function,
                          std::unique_ptr<DominanceFunction<ResourceType>> dominance_function) {
            constexpr size_t ResourceTypeIndex =
                ComponentTypeIndex_v<ResourceType, ResourceTypes...>;
            using ResourceFactoryType = ResourceFactory<ResourceType>;

            pass_directional_resource_kind(*extension_function,
                                           feasibility_function.get(),
                                           dominance_function.get());

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

            pass_directional_resource_kind(*extension_function,
                                           feasibility_function.get(),
                                           dominance_function.get());

            resource_factory_.template add_resource_factory<ResourceTypeIndex, ResourceType>(
                std::make_unique<ResourceFactoryType>(std::move(extension_function),
                                                      std::move(feasibility_function),
                                                      std::move(cost_function),
                                                      std::move(dominance_function),
                                                      resource_base_prototype));
        }

        /// @brief Adds a node whose per-node resource is built from explicit initial values.
        ///
        /// The values seed the node resource's value slots (read by @c BellmanFordAlgorithm
        /// during preprocessing); they are not a label's starting state.
        ///
        /// @param node_id              Identifier of the node to add.
        /// @param resource_initializer Per-type vectors of component initialiser tuples.
        /// @param source               Whether the node is a source.
        /// @param sink                 Whether the node is a sink.
        /// @return Reference to the newly added node.
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

        /// @brief Builds an algorithm on this graph's resources.
        ///
        /// Given only its params, the algorithm searches in `params.direction`: this returns the
        /// class that does, behind its base class. Given other constructor arguments, it returns
        /// @p AlgorithmType itself.
        ///
        /// @tparam AlgorithmType      The algorithm template, e.g. @c SimpleDominanceAlgorithm.
        /// @tparam LabelContainerType The per-node label container.
        /// @tparam CostResourceType   The cost resource type.
        /// @tparam CriticalResourceType  The resource type whose value decides where a
        /// bidirectional
        ///                            search's two halves stop (its half-way point); defaults to
        ///                            @p CostResourceType.
        /// @param args The constructor arguments after the resource factory.
        /// @return The algorithm: a @c std::unique_ptr<Algorithm<R, LabelContainerType>> when
        ///         @p args is the params alone.
        /// @throws std::invalid_argument when @p AlgorithmType cannot search in the direction the
        ///         params ask for.
        template <template <typename, typename> class AlgorithmType,
                  typename LabelContainerType = LabelList<ResourceCompositionType>,
                  typename CostResourceType = RealResource,
                  typename CriticalResourceType = CostResourceType, typename... Args>
        auto create_algorithm(Args&&... args) {
            if constexpr (detail::is_params_only_v<LabelContainerType, Args...>) {
                // Only the params: the class to build depends on params.direction, known at run
                // time (forward builds AlgorithmType; backward and bidirectional build other
                // classes), so this returns their common base, Algorithm<ResourceType,
                // LabelContainerType>.
                return detail::make_directional_algorithm<AlgorithmType,
                                                          ResourceCompositionType,
                                                          LabelContainerType,
                                                          CostResourceType,
                                                          CriticalResourceType>(
                    &resource_factory_,
                    std::forward<Args>(args)...);
            } else {
                // Extra constructor arguments, which only AlgorithmType takes: build it directly,
                // and return its own type.
                return std::make_unique<AlgorithmType<ResourceCompositionType, LabelContainerType>>(
                    &resource_factory_,
                    std::forward<Args>(args)...);
            }
        }

        /// @brief Builds an @p AlgorithmType algorithm and solves the graph with it.
        ///
        /// The search runs in `params.direction`. A backward or bidirectional search first runs
        /// the model checks (see @ref check_model).
        ///
        /// @tparam AlgorithmType      The algorithm template, e.g. @c SimpleDominanceAlgorithm.
        /// @tparam CostResourceType   The numerical resource type holding the cost slot.
        /// @tparam LabelContainerType The per-node label container.
        /// @tparam CriticalResourceType  The resource type whose value decides where a
        /// bidirectional
        ///                            search's two halves stop (its half-way point); defaults to
        ///                            @p CostResourceType.
        /// @param upper_bound Cost upper bound; solutions above it are discarded.
        /// @param params      The algorithm's parameters, including the search direction.
        /// @param preprocess  Whether to reduce the graph before solving.
        /// @param cost_index  Index of the cost component to use.
        /// @return The solutions found and the search's status; empty if the graph has no source
        ///         or no sink, or another solve is running.
        /// @throws std::invalid_argument when @p AlgorithmType cannot search in `params.direction`.
        /// @throws ModelRefused when the model checks find a problem; the graph is left unchanged.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename CostResourceType = RealResource,
                  typename LabelContainerType = LabelList<ResourceCompositionType>,
                  typename CriticalResourceType = CostResourceType>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(
            double upper_bound = std::numeric_limits<double>::infinity(),
            AlgorithmParams<LabelContainerType> params = AlgorithmParams<LabelContainerType>(),
            bool preprocess = true, size_t cost_index = 0) {
            auto algorithm = create_algorithm<AlgorithmType,
                                              LabelContainerType,
                                              CostResourceType,
                                              CriticalResourceType>(std::move(params));
            return solve<Algorithm<ResourceCompositionType, LabelContainerType>, CostResourceType>(
                algorithm.get(),
                upper_bound,
                preprocess,
                cost_index);
        }

        /// @brief The solve above, with no cost upper bound.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename CostResourceType = RealResource,
                  typename LabelContainerType = LabelList<ResourceCompositionType>,
                  typename CriticalResourceType = CostResourceType>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(AlgorithmParams<LabelContainerType> params, bool preprocess = true,
                          size_t cost_index = 0) {
            return solve<AlgorithmType, CostResourceType, LabelContainerType, CriticalResourceType>(
                std::numeric_limits<double>::infinity(),
                std::move(params),
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
                  typename LabelContainerType = LabelList<ResourceCompositionType>,
                  typename CriticalResourceType = CostResourceType>
            requires is_numerical_resource_v<CostResourceType>
        SolveResult solve(AlgorithmBaseParams base_params,
                          double upper_bound = std::numeric_limits<double>::infinity(),
                          bool preprocess = true, size_t cost_index = 0) {
            return solve<AlgorithmType, CostResourceType, LabelContainerType, CriticalResourceType>(
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

            // Checks first, then (with preprocess) the reductions. PreSolvePlan inspects every
            // stage before running any, and undoes the ones that ran however the solve ends. After
            // an exception the graph stays marked modified, so the next solve preprocesses it
            // afresh.
            PreSolvePlan<ResourceCompositionType> stages(algorithm->direction());
            if (preprocess) {
                add_reductions<CostResourceType>(&stages, upper_bound, cost_index);
            }
            SolveContext context{.upper_bound = upper_bound};
            stages.enforce(*this, context);
            stages.run(*this, context);

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
            stages.undo();
            if (preprocess) {
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

        /// @brief Runs the model checks a search in @p direction needs, without solving.
        ///
        /// A forward search needs none. A backward search needs the model's backward semantics
        /// (@c BackwardExtensionCheck); a bidirectional one also needs its join
        /// (@c JoinCheck). The checks read the whole model, arcs that preprocessing removed
        /// included.
        ///
        /// @param direction The search's direction.
        /// @return One line per problem; empty when the model is ready for that search.
        [[nodiscard]] ModelReport check_model(SearchDirection direction) const {
            SolveContext context;
            PreSolvePlan<ResourceCompositionType>(direction).inspect(*this, context);
            return std::move(context.report);
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

        /// @brief Tells a resource's dominance and feasibility functions how its extension function
        ///        extends backwards.
        ///
        /// The extension's directional resource kind decides how the other functions read the
        /// resource backward: the dominance function reverses where @ref reverses_back_dominance
        /// says (a threshold: a larger limit is better), and the feasibility function records the
        /// kind to pick its join test. Set on the prototypes, so every per-node clone inherits
        /// both.
        ///
        /// @param extension_function   The resource's extension function.
        /// @param feasibility_function Receives the kind.
        /// @param dominance_function   Receives whether the backward order reverses.
        template <typename ResourceType>
        static void pass_directional_resource_kind(
            const ExtensionFunction<ResourceType>& extension_function,
            FeasibilityFunction<ResourceType>* feasibility_function,
            DominanceFunction<ResourceType>* dominance_function) {
            const DirectionalResourceKind kind = extension_function.directional_resource_kind();
            dominance_function->set_backward_reversed(reverses_back_dominance(kind));
            feasibility_function->set_directional_resource_kind(kind);
        }

        /// @brief The feasibility preprocessor as a solve runs it: its removals are kept for good,
        ///        and the connectivity matrix is brought up to date after them.
        ///
        /// Nothing to undo: an arc that no feasible extension uses never becomes usable.
        class FeasibilityReduction final : public FeasibilityPreprocessor<ResourceCompositionType> {
            public:
                explicit FeasibilityReduction(ResourceGraph* graph)
                    : FeasibilityPreprocessor<ResourceCompositionType>(&graph->resource_factory_,
                                                                       graph),
                      graph_(graph) {}

                void run(Graph<ResourceCompositionType>& graph, SolveContext& context) override {
                    FeasibilityPreprocessor<ResourceCompositionType>::run(graph, context);
                    graph_->connectivityMatrix_.compute_bitmatrix();
                }

                void undo() noexcept override {}

            private:
                ResourceGraph* graph_;
        };

        /// @brief The shortest-path preprocessor of the graph's resources.
        template <typename CostResourceType>
        using ShortestPathBase = ShortestPathPreprocessor<CostResourceType, ResourceTypes...>;

        /// @brief The shortest-path preprocessor as a solve runs it: the nodes are sorted by
        ///        connectivity first if they are not sorted yet; its removals are undone after the
        ///        search.
        ///
        /// @tparam CostResourceType The numerical resource type holding the cost slot.
        template <typename CostResourceType>
        class ShortestPathReduction final : public ShortestPathBase<CostResourceType> {
            public:
                ShortestPathReduction(ResourceGraph* graph, double upper_bound, size_t cost_index)
                    : ShortestPathBase<CostResourceType>(graph, upper_bound, cost_index),
                      graph_(graph),
                      cost_index_(cost_index) {}

                void run(Graph<ResourceCompositionType>& graph, SolveContext& context) override {
                    // Sort on cost slot cost_index, which the preprocessor and the search read,
                    // not on arc.cost: the two differ once update_reduced_costs has rewritten
                    // that slot.
                    if (!graph_->are_nodes_sorted()) {
                        graph_->template sort_nodes_by_connectivity<ShortestPathConnectivitySort,
                                                                    CostResourceType>(cost_index_);
                    }
                    ShortestPathBase<CostResourceType>::run(graph, context);
                }

            private:
                ResourceGraph* graph_;
                size_t cost_index_;
        };

        /// @brief Adds to @p stages the reductions a solve that may reduce the graph runs: the
        ///        feasibility reduction when the graph was modified, and the shortest-path
        ///        reduction when the cost slot exists.
        ///
        /// @tparam CostResourceType The numerical resource type holding the cost slot.
        /// @param stages      The solve's stages.
        /// @param upper_bound The solve's cost upper bound.
        /// @param cost_index  The cost slot, within @p CostResourceType's components.
        template <typename CostResourceType>
        void add_reductions(PreSolvePlan<ResourceCompositionType>* stages, double upper_bound,
                            size_t cost_index) {
            if (this->is_modified()) {
                stages->add(std::make_unique<FeasibilityReduction>(this));
            }
            // shortest-path preprocessing requires a numerical cost resource in the pack.
            // Use ComponentTypeIndex<...>::value rather than the _v alias: the _v alias is
            // a constrained variable template that is undeclared when the type isn't in the
            // pack, which would make this condition ill-formed (even inside if constexpr).
            if constexpr (is_numerical_resource_v<CostResourceType> &&
                          ComponentTypeIndex<CostResourceType, ResourceTypes...>::value != -1) {
                const size_t slots =
                    resource_factory_.template get_num_resource_type<CostResourceType>();
                if (cost_index < slots) {
                    stages->add(
                        std::make_unique<ShortestPathReduction<CostResourceType>>(this,
                                                                                  upper_bound,
                                                                                  cost_index));
                } else if (cost_index > 0) {
                    LOG_WARN(
                        "ResourceGraph::solve: cost_index is out of bounds for the number of "
                        "extender components of the cost resource. ",
                        cost_index,
                        " for a length of ",
                        slots);
                }
            }
        }

        ResourceCompositionFactory<ResourceTypes...> resource_factory_;
        ConnectivityMatrix<ResourceCompositionType> connectivityMatrix_;
        std::mutex mutex_;
};
}  // namespace rcspp
