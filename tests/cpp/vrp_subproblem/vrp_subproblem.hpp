#pragma once

#include <optional>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

using namespace rcspp;

using RGraph = ResourceGraph<RealResource, IntResource>;

class VRPSubproblem {
        // Solve one iteration of the subproblem of the VRPTW

    public:
        VRPSubproblem(Instance instance,
                      const std::map<size_t, double>* row_coefficient_by_id = nullptr);

        // Given a the duals by node id, solve the subproblem and return a vector of solutions.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        std::vector<Solution> solve(const std::map<size_t, double>& dual_by_id,
                                    AlgorithmBaseParams params = AlgorithmBaseParams()) {
            LOG_TRACE(__FUNCTION__, '\n');

            total_subproblem_time_.start();
            auto solutions_rcspp = solve_with_rcspp<AlgorithmType>(dual_by_id, std::move(params));
            total_subproblem_time_.stop();

            if (!solutions_rcspp.empty()) {
                LOG_DEBUG("Solution RCSPP cost: ", solutions_rcspp[0].cost, '\n');
            }

            LOG_DEBUG("\n", std::string(45, '*'), "\n");
            LOG_DEBUG("total_subproblem_time_: ", total_subproblem_time_.elapsed_seconds());
            LOG_DEBUG("\n", std::string(45, '*'), "\n");

            return solutions_rcspp;
        }

        // Test helper (H3 regression): solve via an externally-owned algorithm so the caller can
        // inspect the label pool's prev_label/ref_count bookkeeping after the solve. Returns true
        // iff the pool is internally consistent; writes the best solution cost to *out_cost when a
        // solution is found. Mirrors solve_with_rcspp()'s default (infinite upper bound) path.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        bool solve_and_check_ref_counts(const std::map<size_t, double>& dual_by_id,
                                        double* out_cost = nullptr) {
            using RC = ResourceTypeComposition<RealResource, IntResource>;
            if (graph_.get_number_of_nodes() == 0) {
                construct_resource_graph(&graph_, &dual_by_id);
            } else {
                update_resource_graph(&graph_, &dual_by_id);
            }
            auto algorithm =
                graph_.create_algorithm<AlgorithmType>(AlgorithmParams<LabelList<RC>>());
            auto result = graph_.solve(algorithm.get());
            if (out_cost != nullptr && !result.solutions.empty()) {
                *out_cost = result.solutions[0].cost;
            }
            return algorithm->get_label_pool().check_ref_count_consistency();
        }

        // Test helper (phase 13): solve through an externally-owned algorithm and report what the
        // solve cost in work, not just what it found. num_extended_labels lives on the algorithm
        // rather than on SolveResult, and the label pool's size is the closest thing to a peak
        // label count, so both need the algorithm object to still be alive afterwards.
        struct RunMeasurement {
                double cost = 0.0;
                size_t solutions = 0;
                size_t extended_labels = 0;
                size_t pooled_labels = 0;
                bool ref_counts_consistent = false;
                // A truncated solve extends fewer labels and so reads as a win. Carrying the
                // status is what stops a timeout being reported as a speedup.
                AlgorithmStatus status = AlgorithmStatus::COMPLETE;
                // Bidirectional only; left at their defaults by every other algorithm. Without
                // these a run whose half-way bound quietly switched itself off would be reported
                // as a measurement OF the bound.
                bool bounded_by_half_way = false;
                size_t joined_paths = 0;
        };

        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        RunMeasurement solve_and_measure(const std::map<size_t, double>& dual_by_id,
                                         AlgorithmBaseParams params = AlgorithmBaseParams()) {
            using RC = ResourceTypeComposition<RealResource, IntResource>;
            if (graph_.get_number_of_nodes() == 0) {
                construct_resource_graph(&graph_, &dual_by_id);
            } else {
                update_resource_graph(&graph_, &dual_by_id);
            }
            // Kept alive so the pool size means "labels allocated at the peak" rather than
            // "zero, because release_label_memory() already ran". The measurement is of search
            // work, not of the release path, which the rest of the suite covers.
            params.release_after_solve = false;
            auto algorithm =
                graph_.create_algorithm<AlgorithmType>(params.with_container(LabelList<RC>()));
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

        // Test helper (H4): expose the full SolveResult — solutions + exit status — so tests can
        // verify solve() reports the status the VRP column-generation loop relies on: COMPLETE when
        // the pricing search was exhaustive vs MEMORY_LIMIT / TIMEOUT / ... when it was cut short.
        // A wrong status would let CG mistake a cut-short solve for a proof of optimality.
        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        SolveResult solve_result(const std::map<size_t, double>& dual_by_id,
                                 AlgorithmBaseParams params = AlgorithmBaseParams()) {
            if (graph_.get_number_of_nodes() == 0) {
                construct_resource_graph(&graph_, &dual_by_id);
            } else {
                update_resource_graph(&graph_, &dual_by_id);
            }
            return graph_.solve<AlgorithmType>(std::move(params));
        }

    private:
        const std::map<size_t, double>* row_coefficient_by_id_;

        Instance instance_;

        size_t path_id_;

        std::map<size_t, std::pair<double, double>> time_window_by_customer_id_;

        RGraph graph_;

        size_t depot_id_;

        Timer total_subproblem_time_;

        std::map<size_t, std::pair<double, double>> initialize_time_windows();

        void construct_resource_graph(RGraph* resource_graph,
                                      const std::map<size_t, double>* dual_by_id = nullptr);

        void update_resource_graph(RGraph* resource_graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_all_nodes_to_graph(RGraph* graph);

        void add_all_arcs_to_graph(RGraph* graph, const std::map<size_t, double>* dual_by_id);

        void add_arc_to_graph(RGraph* graph, size_t customer_orig_id, size_t customer_dest_id,
                              const Customer& customer_orig, const Customer& customer_dest,
                              const std::map<size_t, double>* dual_by_id, size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm>
        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id,
            AlgorithmBaseParams params = AlgorithmBaseParams()) {
            LOG_TRACE(__FUNCTION__, '\n');

            if (graph_.get_number_of_nodes() == 0) {
                construct_resource_graph(&graph_, &dual_by_id);
            } else {
                update_resource_graph(&graph_, &dual_by_id);
            }

            return std::move(graph_.solve<AlgorithmType>(std::move(params)).solutions);
        }

        [[nodiscard]] static std::map<size_t, double> calculate_dual(
            const std::map<size_t, double>& master_dual_by_var_id,
            const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
            double alpha_base = 0.9999, int alpha_max_iter = 20);
};
