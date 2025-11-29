// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>

#include "cg/master_problem.hpp"
#include "cg/mp_solution.hpp"
#include "cg/path.hpp"
#include "instance.hpp"
#include "rcspp/rcspp.hpp"
#include "solution_output.hpp"

using namespace rcspp;

using RGraph = ResourceGraph<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;

class VRP {
    public:
        VRP(Instance instance);

        VRP(Instance instance, std::string duals_directory);

        const std::vector<Path>& generate_initial_paths();

        MPSolution solve(
            std::optional<size_t> subproblem_max_nb_solutions = std::nullopt,
            bool use_boost = false,
            std::optional<std::map<size_t, double>> optimal_dual_by_var_id = std::nullopt);

        template <template <typename> class... AlgorithmTypes>
        std::vector<Timer> solve(AlgorithmParams params = AlgorithmParams{}) {  // NOLINT
            LOG_TRACE(__FUNCTION__, '\n');

            generate_initial_paths();
            MasterProblem master_problem(instance_.get_demand_customers_id());
            master_problem.construct_model(paths_);
            MPSolution master_solution;

            double min_reduced_cost = -std::numeric_limits<double>::infinity();
            std::vector<Timer> timers(1 + sizeof...(AlgorithmTypes));
            int nb_iter = 0;
            while (min_reduced_cost < -EPSILON) {
                master_solution = master_problem.solve();

                const auto dual_by_id =
                    calculate_dual(master_solution.dual_by_var_id, std::nullopt, nb_iter);

                std::vector<Solution> solutions_boost;
                timers.front().start();
                solutions_boost = solve_with_boost(dual_by_id);
                timers.front().stop();

                // Run RCSPP for each AlgorithmType and collect the first algorithm's solutions
                std::vector<Solution> solutions_rcspp_any;
                size_t algo_index = 1;  // timers[0] used by boost
                (void)std::initializer_list<int>{([&]() {
                    timers[algo_index].start();
                    auto sols = solve_with_rcspp<AlgorithmTypes>(dual_by_id, params);
                    timers[algo_index].stop();

                    if (!solutions_boost.empty()) {
                        if (!sols.empty()) {
                            // RCSPP can be better as it uses int for some resources (e.g., load,
                            // time)
                            if (params.stop_after_X_solutions > 0 &&
                                abs(sols[0].cost - solutions_boost[0].cost) >
                                    COST_COMPARISON_EPSILON) {
                                LOG_ERROR("RCSPP solution is different from BOOST (",
                                          algo_index,
                                          ") solution: ",
                                          sols[0].cost,
                                          " vs ",
                                          solutions_boost[0].cost,
                                          "\n");
                            }
                        } else {
                            LOG_ERROR("BOOST has a solution while RCSPP (", algo_index, ") not\n");
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
                        if (algo_index > 1 && sols.size() != solutions_rcspp_any.size()) {
                            LOG_WARN("The number of solutions from RCSPP (algo ",
                                     algo_index,
                                     ") differs from that of the first algorithm: ",
                                     sols.size(),
                                     " vs ",
                                     solutions_rcspp_any.size(),
                                     "\n");
                        }
                    } else {
                        LOG_DEBUG("Solution RCSPP (algo ", algo_index, ") returned no solutions\n");
                    }

                    if (algo_index == 1) {
                        solutions_rcspp_any = sols;
                    }
                    ++algo_index;
                    return 0;
                }())...};

                // If we didn't get any rcsp solutions from first algorithm, ensure we have
                // something to inspect
                if (solutions_rcspp_any.empty()) {
                    LOG_WARN(
                        "No RCSPP solutions from first algorithm; aborting column generation "
                        "loop\n");
                    break;
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

                add_paths(&master_problem, negative_red_cost_solutions);

                nb_iter++;

                LOG_DEBUG(std::string(45, '*'), '\n');
                LOG_INFO("nb_iter=",
                         nb_iter,
                         " | obj=",
                         master_solution.cost,
                         " | min_reduced_cost=",
                         std::fixed,
                         std::setprecision(std::numeric_limits<double>::max_digits10),
                         min_reduced_cost,
                         " | paths_generated=",
                         negative_red_cost_solutions.size(),
                         " | EPSILON=",
                         EPSILON,
                         '\n');
                LOG_DEBUG(std::string(45, '*'), '\n');
            }

            return timers;
        }

        void sort_nodes();
        void sort_nodes_by_connectivity();
        void sort_nodes_by_min_tw();
        void sort_nodes_by_max_tw();

        [[nodiscard]] const std::vector<Path>& get_paths() const;

    private:
        static constexpr double COST_COMPARISON_EPSILON = 1e-6;

        Instance instance_;

        std::map<size_t, double> min_time_window_by_node_id_;
        std::map<size_t, double> max_time_window_by_node_id_;

        std::map<size_t, std::set<size_t>> node_set_by_node_id_;

        size_t path_id_ = 0;

        std::map<size_t, std::pair<int, int>> time_window_by_customer_id_;
        std::map<size_t, std::set<size_t>> ng_neighborhood_customer_id_;

        // Resource graph. needs to be loaded after time windows and ng neighborhoods are
        // initialized
        RGraph graph_;

        std::optional<SolutionOutput> solution_output_;

        size_t depot_id_;

        std::vector<Path> paths_;

        Timer total_subproblem_time_;
        Timer total_subproblem_solve_time_;

        Timer total_subproblem_time_boost_;
        Timer total_subproblem_solve_time_boost_;

        std::vector<std::vector<double>> distances_;

        std::map<size_t, std::pair<int, int>> initialize_time_windows();
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
                                     const std::map<size_t, double>* dual_by_id, size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(MasterProblem* master_problem, const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        template <template <typename> class AlgorithmType = SimpleDominanceAlgorithmIterators>
        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id, AlgorithmParams params = {}) {
            LOG_TRACE(__FUNCTION__, '\n');

            update_resource_graph(&graph_, &dual_by_id);
            total_subproblem_solve_time_.start();
            auto solutions = graph_.solve<AlgorithmType>(-EPSILON, params);

            LOG_DEBUG(__FUNCTION__,
                      " Time: ",
                      total_subproblem_solve_time_.elapsed_milliseconds(/* only_current = */ true),
                      " (ms)\n");

            total_subproblem_solve_time_.stop();

            return solutions;
        }

        [[nodiscard]] std::vector<Solution> solve_with_boost(
            const std::map<size_t, double>& dual_by_id);

        [[nodiscard]] static std::map<size_t, double> calculate_dual(
            const std::map<size_t, double>& master_dual_by_var_id,
            const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
            double alpha_base = 0.9999, int alpha_max_iter = 20);

        const double EPSILON = 0.00000001;
};
