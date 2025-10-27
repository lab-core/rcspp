#pragma once

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

#include <optional>


using namespace rcspp;

class VRPSubproblem {
    // Solve one iteration of the subproblem of the VRPTW

    public:
        VRPSubproblem(Instance instance,
                  const std::map<size_t, double>* row_coefficient_by_id = nullptr);

        // Given a the duals by node id, solve the subproblem and return a vector of solutions.
        std::vector<Solution> solve(const std::map<size_t, double>& dual_by_id);

    private:

        const std::map<size_t, double>* row_coefficient_by_id_;

        Instance instance_;

        std::map<size_t, double> min_time_window_by_arc_id_;
        std::map<size_t, double> max_time_window_by_node_id_;

        size_t path_id_;

        std::map<size_t, std::pair<double, double>> time_window_by_customer_id_;

        ResourceGraph<RealResource> initial_graph_;

        ResourceGraph<RealResource> subproblem_graph_;

        size_t depot_id_;

        int64_t total_subproblem_time_ = 0;

        std::map<size_t, std::pair<double, double>> initialize_time_windows();

        ResourceGraph<RealResource> construct_resource_graph(
            const std::map<size_t, double>* dual_by_id = nullptr);

        void update_resource_graph(ResourceGraph<RealResource>* resource_graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_all_nodes_to_graph(ResourceGraph<RealResource>* graph);

        void add_all_arcs_to_graph(ResourceGraph<RealResource>* graph,
                                   const std::map<size_t, double>* dual_by_id);

        void add_arc_to_graph(ResourceGraph<RealResource>* graph, size_t customer_orig_id,
                                     size_t customer_dest_id, const Customer& customer_orig,
                                     const Customer& customer_dest,
                                     const std::map<size_t, double>* dual_by_id, size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        [[nodiscard]] std::vector<Solution> solve_with_rcspp(
            const std::map<size_t, double>& dual_by_id);

        [[nodiscard]] static std::map<size_t, double> calculate_dual(
            const std::map<size_t, double>& master_dual_by_var_id,
            const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
            double alpha_base = 0.9999, int alpha_max_iter = 20);
};