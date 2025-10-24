// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>

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
        VRP(Instance instance, SolutionOutput* solution_output = nullptr);

        const std::vector<Path>& generate_initial_paths();

        MPSolution solve(
            std::optional<size_t> subproblem_max_nb_solutions = std::nullopt,
            bool use_boost = false,
            std::optional<std::map<size_t, double>> optimal_dual_by_var_id = std::nullopt);

        [[nodiscard]] const std::vector<Path>& get_paths() const;

    private:
        static constexpr double COST_COMPARISON_EPSILON = 1e-6;

        Instance instance_;

        std::map<size_t, double> min_time_window_by_arc_id_;
        std::map<size_t, double> max_time_window_by_node_id_;

        size_t path_id_ = -1;

        std::map<size_t, std::pair<double, double>> time_window_by_customer_id_;

        ResourceGraph<RealResource> initial_graph_;

        ResourceGraph<RealResource> subproblem_graph_;

        SolutionOutput* solution_output_;

        size_t depot_id_;

        std::vector<Path> paths_;

        int64_t total_subproblem_time_ = 0;
        int64_t total_subproblem_solve_time_ = 0;

        int64_t total_subproblem_time_boost_ = 0;
        int64_t total_subproblem_solve_time_boost_ = 0;

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

        void update_all_arcs(ResourceGraph<RealResource>* graph,
                             const std::map<size_t, double>* dual_by_id);

        void update_arc(ResourceGraph<RealResource>* graph, size_t customer_orig_id,
                        size_t customer_dest_id, const Customer& customer_orig,
                        const Customer& customer_dest, const std::map<size_t, double>* dual_by_id,
                        size_t arc_id);

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);

        void add_paths(const std::vector<Solution>& solutions);

        [[nodiscard]] double calculate_solution_cost(const Solution& solution) const;

        std::vector<Solution> solve_with_rcspp(const std::map<size_t, double>& dual_by_id);

        [[nodiscard]] std::vector<Solution> solve_with_boost(
            const std::map<size_t, double>& dual_by_id);

        [[nodiscard]] static std::map<size_t, double> calculate_dual(
            const std::map<size_t, double>& master_dual_by_var_id,
            const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
            double alpha_base = 0.9999, int alpha_max_iter = 20);

        const double EPSILON = 0.00000001;
};
