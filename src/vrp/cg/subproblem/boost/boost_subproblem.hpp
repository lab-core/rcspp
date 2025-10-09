// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "graph_vrptw.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "vrp/instance.hpp"

class BoostSubproblem {
    public:
        BoostSubproblem(Instance instance, const std::map<size_t, double>* dual_by_id = nullptr);

        std::vector<Solution> solve();

    private:
        Instance instance_;
        const std::map<size_t, double>* dual_by_id_;

        size_t source_id_;
        size_t sink_id_;

        GraphVRPTW graph_boost_;

        GraphVRPTW construct_boost_graph();

        void add_vertices(GraphVRPTW& graph_boost);

        void add_edges(GraphVRPTW& graph_boost) const;

        void add_single_edge(size_t arc_id, const Customer& customer_orig,
                             const Customer& customer_dest, GraphVRPTW& graph_boost) const;

        [[nodiscard]] static double calculate_distance(const Customer& customer1,
                                                       const Customer& customer2);
};
