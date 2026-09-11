// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The VRP pricing subproblem with an ng-path component: a *third* type slot.
//
// A parallel class rather than a template parameter on `VRPSubproblem`, for the reason
// `09-ng-path-benchmark.md` §2.5 gives for the instance generator: the existing harness is the
// baseline every number in `analysis/bidirectional-results.md` refers to, and it must keep its
// exact signatures so those numbers stay reproducible. What is duplicated here is graph
// *construction*; what is shared is `Instance`, which is where a divergence would actually
// invalidate the comparison.
//
// **The reference optimum is a different number.** ng-feasibility is a *restriction* -- it forbids
// cycles within each node's neighborhood -- so the set of admissible columns shrinks and the
// optimum becomes less negative than the non-ng model's `kOptimal`. It cannot be corrected by
// inspection; it has to be derived. See `test_bidirectional_benchmark.hpp`.
//
// **The ng arcs carry no per-arc set data.** Step 4's accepted narrowing means
// `NgPathExtensionFunction` derives the node it adds from the arc's own endpoints, so an origin
// singleton supplied here would be ignored. An empty set is the honest initializer.

#include <cstddef>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

/// @brief The three-slot pack: cost and time in the real slot, demand in the int slot, and the
///        ng memory in a bitset slot.
using RGraphNg = ResourceGraph<RealResource, IntResource, SizeTBitsetResource>;

/// @brief The matching composition, for the label container.
using RCNg = ResourceTypeComposition<RealResource, IntResource, SizeTBitsetResource>;

/// @brief One iteration of the VRPTW pricing subproblem, with an ng-path relaxation.
class VRPSubproblemNg {
    public:
        /// @brief Builds the model for @p instance.
        ///
        /// @param instance             The VRP instance.
        /// @param ng_neighborhood_size How many nearest customers form each node's
        ///                             ng-neighborhood. Larger is a tighter relaxation and a
        ///                             harder subproblem; 0 makes the component inert, which is
        ///                             what makes an ng-versus-no-ng comparison a controlled one.
        explicit VRPSubproblemNg(Instance instance, size_t ng_neighborhood_size = 8);

        /// @brief Solves the subproblem and returns the solutions.
        ///
        /// @tparam AlgorithmType The algorithm template to run.
        /// @param dual_by_id The duals, by customer id.
        /// @param params     Algorithm parameters.
        /// @return The solutions found.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        std::vector<Solution> solve(const std::map<size_t, double>& dual_by_id,
                                    AlgorithmBaseParams params = AlgorithmBaseParams()) {
            if (graph_.get_number_of_nodes() == 0) {
                construct_resource_graph(&graph_, &dual_by_id);
            } else {
                update_resource_graph(&graph_, &dual_by_id);
            }
            return graph_.solve<AlgorithmType>(std::move(params)).solutions;
        }

        /// @brief What the solve found *and* what it cost in work.
        ///
        /// Mirrors @c VRPSubproblem::RunMeasurement field for field, so the ng and non-ng tables
        /// are directly comparable, and adds the join-pass counter step 6's measurement needs.
        struct RunMeasurement {
                double cost = 0.0;
                size_t solutions = 0;
                size_t extended_labels = 0;
                size_t pooled_labels = 0;
                bool ref_counts_consistent = false;
                AlgorithmStatus status = AlgorithmStatus::COMPLETE;
                bool bounded_by_half_way = false;
                size_t joined_paths = 0;
        };

        /// @brief Solves through an externally-owned algorithm and reports the work done.
        ///
        /// @tparam AlgorithmType The algorithm template to run.
        /// @param dual_by_id The duals, by customer id.
        /// @param params     Algorithm parameters.
        /// @return The measurement.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        RunMeasurement solve_and_measure(const std::map<size_t, double>& dual_by_id,
                                         AlgorithmBaseParams params = AlgorithmBaseParams()) {
            if (graph_.get_number_of_nodes() == 0) {
                construct_resource_graph(&graph_, &dual_by_id);
            } else {
                update_resource_graph(&graph_, &dual_by_id);
            }
            params.release_after_solve = false;
            auto algorithm =
                graph_.create_algorithm<AlgorithmType>(params.with_container(LabelList<RCNg>()));
            const auto result = graph_.solve(algorithm.get());

            RunMeasurement measurement;
            measurement.status = result.status;
            measurement.solutions = result.solutions.size();
            if (!result.solutions.empty()) {
                measurement.cost = result.solutions.front().cost;
            }
            measurement.extended_labels = algorithm->get_number_of_extended_labels();
            measurement.pooled_labels = algorithm->get_label_pool().get_nb_total_labels();
            measurement.ref_counts_consistent =
                algorithm->get_label_pool().check_ref_count_consistency();
            if constexpr (requires { algorithm->bounded_by_half_way(); }) {
                measurement.bounded_by_half_way = algorithm->bounded_by_half_way();
                measurement.joined_paths = algorithm->number_of_joined_paths();
            }
            return measurement;
        }

        /// @brief The ng neighborhoods this model was built with, for a test to inspect.
        ///
        /// @return Map from customer id to its ng-neighborhood.
        [[nodiscard]] const std::map<size_t, std::set<size_t>>& get_ng_neighborhoods() const {
            return ng_neighborhood_by_customer_id_;
        }

    private:
        Instance instance_;
        size_t ng_neighborhood_size_;
        std::map<size_t, std::pair<double, double>> time_window_by_customer_id_;
        std::map<size_t, std::set<size_t>> ng_neighborhood_by_customer_id_;
        std::map<size_t, std::set<size_t>> forbidden_by_customer_id_;
        RGraphNg graph_;
        size_t depot_id_ = 0;

        std::map<size_t, std::pair<double, double>> initialize_time_windows();

        /// @brief Each customer's ng-neighborhood: its @c ng_neighborhood_size_ nearest others.
        ///
        /// The classic ng-route choice. A larger neighborhood forbids more cycles, so it is a
        /// tighter relaxation and a harder subproblem; 0 leaves every neighborhood empty, which
        /// makes the component present but inert.
        std::map<size_t, std::set<size_t>> initialize_ng_neighborhoods();

        void construct_resource_graph(RGraphNg* resource_graph,
                                      const std::map<size_t, double>* dual_by_id = nullptr);

        void update_resource_graph(RGraphNg* resource_graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_all_nodes_to_graph(RGraphNg* graph);

        void add_all_arcs_to_graph(RGraphNg* graph, const std::map<size_t, double>* dual_by_id);

        void add_arc_to_graph(RGraphNg* graph, size_t customer_orig_id, size_t customer_dest_id,
                              const Customer& customer_orig, const Customer& customer_dest,
                              const std::map<size_t, double>* dual_by_id, size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);
};
