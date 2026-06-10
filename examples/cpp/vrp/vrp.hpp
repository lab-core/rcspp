// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <functional>
#include <limits>
#include <optional>

#include "cg/master_problem.hpp"
#include "cg/mp_solution.hpp"
#include "cg/path.hpp"
#include "instance.hpp"
#include "rcspp/rcspp.hpp"
#include "solution_output.hpp"

using namespace rcspp;

using RGraph = ResourceGraph<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;
using ResourceType =
    ResourceTypeComposition<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;

/// @brief Result of a column-generation VRP::solve() run.
struct CGSolveResult {
        /// @brief Per-algorithm timing in the same order as the algorithm parameters.
        std::vector<Timer> timers;
        /// @brief Final LP relaxation cost from the master problem after column generation.
        double lp_cost = std::numeric_limits<double>::infinity();
        /// @brief False when CG stopped on "no improving column" but the final pricing solve did
        /// not run to completion (timeout / memory / phase / solution cap) — the bound is then
        /// valid but not proven optimal.
        bool proven_optimal = true;
};

/// @brief Type-erased solver for passing heterogeneous-container algorithms to VRP::solve.
///
/// Wrap any algorithm whose LabelContainerType differs from the primary one using
/// run_algorithm() so it participates in each CG iteration alongside the main algorithms.
struct ExtraSolver {
        /// @brief Callable invoked at every CG iteration with the current dual values.
        std::function<std::vector<Solution>(const std::map<size_t, double>&)> fn;
        /// @brief Whether fn always finds the optimal solution (used for cross-checking).
        bool optimal = true;
};

class VRP {
    public:
        VRP(Instance instance);

        VRP(Instance instance, std::string duals_directory);

        const std::vector<Path>& generate_initial_paths();

        MPSolution solve(
            std::optional<size_t> subproblem_max_nb_solutions = std::nullopt,
            bool use_boost = false,
            std::optional<std::map<size_t, double>> optimal_dual_by_var_id = std::nullopt);

        template <template <typename, typename> class... AlgorithmTypes,
                  typename LabelContainerType>
        CGSolveResult solve(                             // NOLINT
            AlgorithmParams<LabelContainerType> params,  // NOLINT
            std::optional<size_t> numAlgos = std::nullopt,
            std::vector<Algorithm<ResourceType, LabelContainerType>*> algorithms = {},
            bool run_boost = false, std::vector<ExtraSolver> extra_solvers = {}) {  // NOLINT
            LOG_TRACE(__FUNCTION__, '\n');

#ifndef RCSPP_VRP_HAS_BOOST
            if (run_boost) {
                LOG_WARN("Boost is not compiled in; setting run_boost to false.\n");
                run_boost = false;
            }
#endif
            size_t num_total_algos = sizeof...(AlgorithmTypes) + (run_boost ? 1 : 0) +
                                     algorithms.size() + extra_solvers.size();

            if (numAlgos.has_value()) {
                size_t nAlgos = numAlgos.value();
                if (nAlgos != num_total_algos) {
                    LOG_ERROR("There is not the right number of algorithms defined.\n");
                    return {};
                }
            }

            generate_initial_paths();
            MasterProblem master_problem(instance_.get_demand_customers_id());
            master_problem.construct_model(paths_);
            MPSolution master_solution;

            double min_reduced_cost = -std::numeric_limits<double>::infinity();
            std::vector<Timer> timers(num_total_algos);
            int nb_iter = 0;
            bool proven_optimal = true;
            while (min_reduced_cost < -EPSILON) {
                master_solution = master_problem.solve();

                const auto dual_by_id =
                    calculate_dual(master_solution.dual_by_var_id, std::nullopt, nb_iter);

                std::vector<Solution> solutions_boost;
#ifdef RCSPP_VRP_HAS_BOOST
                if (run_boost) {
                    timers.front().start();
                    solutions_boost = solve_with_boost(dual_by_id);
                    timers.front().stop();
                }
#endif

                // Run RCSPP for each AlgorithmType and collect the first algorithm's solutions
                std::vector<Solution> solutions_rcspp_any;
                const size_t first_rcspp_idx = run_boost ? 1 : 0;
                size_t algo_index = first_rcspp_idx;
                // Exit status of the solve whose result drives min_reduced_cost (the first RCSPP
                // algorithm). If that solve was cut short, a "no improving column" result does not
                // prove optimality. (Extra solvers are type-erased and report no status.)
                AlgorithmStatus first_rcspp_status = AlgorithmStatus::COMPLETE;

                auto collect_solutions = [&](std::vector<Solution>& sols, bool is_optimal) {
                    bool non_optimal = !is_optimal;
                    if (!solutions_boost.empty()) {
                        if (!sols.empty()) {
                            // RCSPP can be better as it uses int for some resources (e.g., load,
                            // time)
                            double diff = solutions_boost[0].cost - sols[0].cost;
                            if ((non_optimal && diff > COST_COMPARISON_EPSILON) ||
                                (!non_optimal && abs(diff) > COST_COMPARISON_EPSILON)) {
                                LOG_ERROR("RCSPP solution is not coherent with BOOST (",
                                          algo_index,
                                          ") solution: ",
                                          sols[0].cost,
                                          " vs ",
                                          solutions_boost[0].cost,
                                          "\n");
                            }
                        } else if (solutions_boost[0].cost < -EPSILON) {
                            if (!non_optimal) {
                                LOG_ERROR("BOOST has a solution while RCSPP (",
                                          algo_index,
                                          ") not\n");
                            }
                        }
                    }

                    if (!sols.empty()) {
                        LOG_DEBUG("Solution RCSPP (algo ",
                                  algo_index,
                                  "): cost=",
                                  sols[0].cost,
                                  " | nb_solutions=",
                                  sols.size(),
                                  '\n');
                        if (algo_index > first_rcspp_idx && !non_optimal &&
                            !solutions_rcspp_any.empty()) {
                            double diff = abs(sols[0].cost - solutions_rcspp_any[0].cost);
                            if (diff > COST_COMPARISON_EPSILON) {
                                LOG_ERROR("RCSPP (algo ",
                                          algo_index,
                                          ") best cost differs from first algorithm: ",
                                          sols[0].cost,
                                          " vs ",
                                          solutions_rcspp_any[0].cost,
                                          "\n");
                            }
                        }
                    } else {
                        LOG_DEBUG("Solution RCSPP (algo ", algo_index, ") returned no solutions\n");
                        if (algo_index > first_rcspp_idx && !non_optimal &&
                            !solutions_rcspp_any.empty() &&
                            solutions_rcspp_any[0].cost < -EPSILON) {
                            LOG_ERROR("RCSPP (algo ",
                                      algo_index,
                                      ") found no solution while first algorithm did\n");
                        }
                    }

                    if (algo_index == first_rcspp_idx) {
                        solutions_rcspp_any = std::move(sols);
                    }
                    ++algo_index;
                    return 0;
                };

                (void)std::initializer_list<int>{([&]() {
                    timers[algo_index].start();
                    AlgorithmStatus st = AlgorithmStatus::COMPLETE;
                    auto sols = solve_with_rcspp<AlgorithmTypes>(dual_by_id, params, &st);
                    timers[algo_index].stop();
                    if (algo_index == first_rcspp_idx) {
                        first_rcspp_status = st;
                    }
                    return collect_solutions(sols, !params.could_be_non_optimal());
                }())...};

                for (auto* algorithm : algorithms) {
                    timers[algo_index].start();
                    AlgorithmStatus st = AlgorithmStatus::COMPLETE;
                    auto sols = solve_with_rcspp(dual_by_id, algorithm, &st);
                    timers[algo_index].stop();
                    if (algo_index == first_rcspp_idx) {
                        first_rcspp_status = st;
                    }
                    collect_solutions(sols, algorithm->is_optimal());
                }

                for (auto& extra : extra_solvers) {
                    timers[algo_index].start();
                    auto sols = extra.fn(dual_by_id);
                    timers[algo_index].stop();
                    collect_solutions(sols, extra.optimal);
                }

                // Collect negative reduced cost solutions from the chosen RCSPP results
                std::vector<Solution> negative_red_cost_solutions;
                min_reduced_cost = std::numeric_limits<double>::infinity();
                for (const auto& sol : solutions_rcspp_any) {
                    min_reduced_cost = std::min(min_reduced_cost, sol.cost);
                    if (sol.cost < -EPSILON) {
                        negative_red_cost_solutions.push_back(sol);
                    }
                }

                // No improving column this iteration -> CG is about to stop. If the pricing solve
                // that produced this result was cut short, optimality is not proven.
                if (min_reduced_cost >= -EPSILON &&
                    first_rcspp_status != AlgorithmStatus::COMPLETE) {
                    proven_optimal = false;
                    LOG_WARN(
                        "Column generation stopped without proving optimality: the final "
                        "pricing subproblem exited with status '",
                        to_string(first_rcspp_status),
                        "' (not complete). The LP objective ",
                        master_solution.cost,
                        " is a valid bound but is NOT proven optimal.\n");
                }

                add_paths(&master_problem, negative_red_cost_solutions);

                LOG_DEBUG(std::string(45, '*'), '\n');
                LOG_INFO("nb_iter=",
                         nb_iter++,
                         " | obj=",
                         master_solution.cost,
                         " | min_reduced_cost=",
                         std::fixed,
                         std::setprecision(std::numeric_limits<double>::max_digits10),
                         min_reduced_cost,
                         " | paths_generated=",
                         negative_red_cost_solutions.size(),
                         '\n');
                LOG_DEBUG(std::string(45, '*'), '\n');
            }

            return CGSolveResult{timers, master_solution.cost, proven_optimal};
        }

        RGraph& get_graph() { return graph_; }

        /// @brief Runs a single algorithm on the current graph with the given dual values.
        ///
        /// Intended for building ExtraSolver lambdas: the caller creates an algorithm on
        /// get_graph() and captures it in a lambda that calls this method.
        ///
        /// @param dual_by_id Dual values keyed by arc id.
        /// @param algo       Pre-constructed algorithm to run; reused across CG iterations.
        /// @return           Solutions found by the algorithm.
        template <class AlgorithmType>
        [[nodiscard]] std::vector<Solution> run_algorithm(
            const std::map<size_t, double>& dual_by_id, AlgorithmType* algo) {
            return solve_with_rcspp(dual_by_id, algo);
        }

        void sort_nodes();
        void sort_nodes_by_connectivity();
        void sort_nodes_by_min_tw();
        void sort_nodes_by_max_tw();

        [[nodiscard]] const std::vector<Path>& get_paths() const;

    private:
        static constexpr double COST_COMPARISON_EPSILON = 1e-6;

        Instance instance_;
        std::map<size_t, std::set<size_t>> node_set_by_node_id_;

        size_t path_id_ = 0;

        std::map<size_t, std::pair<double, double>> time_window_by_customer_id_;
        std::map<size_t, std::set<size_t>> ng_neighborhood_customer_id_;

        // Resource graph. needs to be loaded after time windows and ng neighborhoods are
        // initialized
        RGraph graph_;

        std::optional<SolutionOutput> solution_output_;

        size_t depot_id_;

        std::vector<Path> paths_;

        Timer total_subproblem_time_;
        Timer total_subproblem_solve_time_;

#ifdef RCSPP_VRP_HAS_BOOST
        Timer total_subproblem_time_boost_;
        Timer total_subproblem_solve_time_boost_;
#endif

        std::vector<std::vector<double>> distances_;

        std::map<size_t, std::pair<double, double>> initialize_time_windows();
        std::map<size_t, std::set<size_t>> initialize_ng_neighborhoods(size_t max_size);

        void construct_resource_graph(RGraph* graph,
                                      const std::map<size_t, double>* dual_by_id = nullptr);

        void update_resource_graph(RGraph* resource_graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_all_nodes_to_graph(RGraph* graph);

        void add_all_arcs_to_graph(RGraph* graph, const std::map<size_t, double>* dual_by_id);

        static void add_arc_to_graph(RGraph* graph, size_t customer_orig_id,
                                     size_t customer_dest_id, const Customer& customer_orig,
                                     const Customer& customer_dest,
                                     const std::map<size_t, double>* dual_by_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(MasterProblem* master_problem, const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        template <template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
                  typename LabelContainerType>
        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id, AlgorithmParams<LabelContainerType> params,
            AlgorithmStatus* out_status = nullptr) {
            LOG_TRACE(__FUNCTION__, '\n');

            update_resource_graph(&graph_, &dual_by_id);
            total_subproblem_solve_time_.start();
            auto result = graph_.solve<AlgorithmType>(-EPSILON, params);

            LOG_DEBUG(__FUNCTION__,
                      " Time: ",
                      total_subproblem_solve_time_.elapsed_milliseconds(/* only_current = */ true),
                      " (ms)\n");

            total_subproblem_solve_time_.stop();

            if (out_status != nullptr) {
                *out_status = result.status;
            }
            return std::move(result.solutions);
        }

        template <class AlgorithmType>
        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id, AlgorithmType* algo,
            AlgorithmStatus* out_status = nullptr) {
            LOG_TRACE(__FUNCTION__, '\n');

            update_resource_graph(&graph_, &dual_by_id);
            total_subproblem_solve_time_.start();
            auto result = graph_.solve(algo, -EPSILON);

            LOG_DEBUG(__FUNCTION__,
                      " Time: ",
                      total_subproblem_solve_time_.elapsed_milliseconds(/* only_current = */ true),
                      " (ms)\n");

            total_subproblem_solve_time_.stop();

            if (out_status != nullptr) {
                *out_status = result.status;
            }
            return std::move(result.solutions);
        }

#ifdef RCSPP_VRP_HAS_BOOST
        [[nodiscard]] std::vector<Solution> solve_with_boost(
            const std::map<size_t, double>& dual_by_id);
#endif

        [[nodiscard]] static std::map<size_t, double> calculate_dual(
            const std::map<size_t, double>& master_dual_by_var_id,
            const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
            double alpha_base = 0.9999, int alpha_max_iter = 20);

        const double EPSILON = 0.00000001;
};
