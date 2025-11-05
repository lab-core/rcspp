// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>

#include "cg/master_problem.hpp"
#include "cg/mp_solution.hpp"
#include "cg/path.hpp"
#include "instance.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "solution_output.hpp"

using namespace rcspp;

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
        std::vector<Timer> solve() {
            LOG_TRACE(__FUNCTION__, '\n');

            generate_initial_paths();
            MasterProblem master_problem(instance_.get_demand_customers_id());
            master_problem.construct_model(paths_);

            double min_reduced_cost = -std::numeric_limits<double>::infinity();
            std::vector<Timer> timers(1 + sizeof...(AlgorithmTypes));
            int nb_iter = 0;
            while (min_reduced_cost < -EPSILON) {
                LOG_DEBUG(std::string(45, '*'), '\n');
                LOG_INFO("nb_iter=",
                         nb_iter,
                         " | min_reduced_cost=",
                         std::fixed,
                         std::setprecision(std::numeric_limits<double>::max_digits10),
                         min_reduced_cost,
                         " | EPSILON=",
                         EPSILON,
                         '\n');
                LOG_DEBUG(std::string(45, '*'), '\n');

                MPSolution master_solution = master_problem.solve();

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
                    auto sols = solve_with_rcspp<AlgorithmTypes>(dual_by_id);
                    timers[algo_index].stop();

                    if (!solutions_boost.empty()) {
                        if (!sols.empty()) {
                            if (std::abs(solutions_boost[0].cost - sols[0].cost) >
                                COST_COMPARISON_EPSILON) {
                                LOG_ERROR("BOOST and RCSPP (",
                                          algo_index,
                                          ") first-solution costs differ beyond tolerance\n");
                            }
                        } else {
                            LOG_ERROR("BOOST has a solution while RCSPP (", algo_index, ") not\n");
                        }
                    }

                    if (!sols.empty()) {
                        LOG_DEBUG("Solution RCSPP cost (algo ",
                                  algo_index,
                                  "): ",
                                  sols[0].cost,
                                  '\n');
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
            }

            return timers;
        }

        [[nodiscard]] const std::vector<Path>& get_paths() const;

    private:
        static constexpr double COST_COMPARISON_EPSILON = 1e-6;

        Instance instance_;

        std::map<size_t, double> min_time_window_by_arc_id_;
        std::map<size_t, double> max_time_window_by_node_id_;

        size_t path_id_ = 0;

        std::map<size_t, std::pair<double, double>> time_window_by_customer_id_;

        ResourceGraph<RealResource> initial_graph_;

        ResourceGraph<RealResource> subproblem_graph_;

        std::optional<SolutionOutput> solution_output_;

        size_t depot_id_;

        std::vector<Path> paths_;

        Timer total_subproblem_time_;
        Timer total_subproblem_solve_time_;

        Timer total_subproblem_time_boost_;
        Timer total_subproblem_solve_time_boost_;

        std::map<size_t, std::pair<double, double>> initialize_time_windows();

        ResourceGraph<RealResource> construct_resource_graph(
            const std::map<size_t, double>* dual_by_id = nullptr);

        void update_resource_graph(ResourceGraph<RealResource>* resource_graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_all_nodes_to_graph(ResourceGraph<RealResource>* graph);

        void add_all_arcs_to_graph(ResourceGraph<RealResource>* graph,
                                   const std::map<size_t, double>* dual_by_id);

        static void add_arc_to_graph(ResourceGraph<RealResource>* graph, size_t customer_orig_id,
                                     size_t customer_dest_id, const Customer& customer_orig,
                                     const Customer& customer_dest,
                                     const std::map<size_t, double>* dual_by_id, size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(MasterProblem* master_problem, const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        template <template <typename> class AlgorithmType = SimpleDominanceAlgorithmIterators>
        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id) {
            LOG_TRACE(__FUNCTION__, '\n');

            if (subproblem_graph_.get_number_of_nodes() == 0) {
                subproblem_graph_ = construct_resource_graph(&dual_by_id);
            } else {
                update_resource_graph(&subproblem_graph_, &dual_by_id);
            }

            total_subproblem_solve_time_.start();
            auto solutions = subproblem_graph_.solve<AlgorithmType>();

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
