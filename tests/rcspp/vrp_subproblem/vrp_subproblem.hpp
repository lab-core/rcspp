#pragma once

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

#include <optional>


using namespace rcspp;

using RGraph = ResourceGraph<RealResource, IntResource>;

class VRPSubproblem {
    // Solve one iteration of the subproblem of the VRPTW

    public:
        VRPSubproblem(Instance instance,
                  const std::map<size_t, double>* row_coefficient_by_id = nullptr);

        // Given a the duals by node id, solve the subproblem and return a vector of solutions.
    template <template <typename> class AlgorithmType = SimpleDominanceAlgorithmIterators>
    std::vector<Solution> solve(const std::map<size_t, double>& dual_by_id) {
        LOG_TRACE(__FUNCTION__, '\n');

        total_subproblem_time_.start();
        auto solutions_rcspp = solve_with_rcspp<AlgorithmType>(dual_by_id);
        total_subproblem_time_.stop();

        LOG_DEBUG("Solution RCSPP cost: ", solutions_rcspp[0].cost, '\n');

        LOG_DEBUG("\n", std::string(45, '*'), "\n");
        LOG_DEBUG("total_subproblem_time_: ", total_subproblem_time_.elapsed_seconds());
        LOG_DEBUG("\n", std::string(45, '*'), "\n");

        return solutions_rcspp;
    }

    private:

        const std::map<size_t, double>* row_coefficient_by_id_;

        Instance instance_;

        std::map<size_t, double> min_time_window_by_node_id_;
        std::map<size_t, double> max_time_window_by_node_id_;

        size_t path_id_;

        std::map<size_t, std::pair<int, int>> time_window_by_customer_id_;

        RGraph graph_;

        size_t depot_id_;

        Timer total_subproblem_time_;

        std::map<size_t, std::pair<int, int>> initialize_time_windows();

        void construct_resource_graph(
        RGraph* resource_graph,
            const std::map<size_t, double>* dual_by_id = nullptr);

        void update_resource_graph(RGraph* resource_graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_all_nodes_to_graph(RGraph* graph);

        void add_all_arcs_to_graph(RGraph* graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_arc_to_graph(RGraph* graph, size_t customer_orig_id,
                                     size_t customer_dest_id, const Customer& customer_orig,
                                     const Customer& customer_dest,
                                     const std::map<size_t, double>* dual_by_id, size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        template <template <typename> class AlgorithmType = SimpleDominanceAlgorithmIterators>
        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id) {
                LOG_TRACE(__FUNCTION__, '\n');

                if (graph_.get_number_of_nodes() == 0) {
                    construct_resource_graph(&graph_, &dual_by_id);
                } else {
                    update_resource_graph(&graph_, &dual_by_id);
                }

                auto solutions = graph_.solve<AlgorithmType>();

                return solutions;
        }

        [[nodiscard]] static std::map<size_t, double> calculate_dual(
            const std::map<size_t, double>& master_dual_by_var_id,
            const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
            double alpha_base = 0.9999, int alpha_max_iter = 20);
};