// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Small pricing graphs, vehicle routing graphs and pricing stages for the column generation
// tests.

#include <cstddef>
#include <memory>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/column_generation/pricing_strategy.hpp"
#include "rcspp/graph/row.hpp"
#include "rcspp/resource/concrete/container_resource.hpp"
#include "rcspp/resource/concrete/functions/cost/value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/extension/addition_extension_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
#include "rcspp/resource/presets.hpp"
#include "rcspp/resource/resource_graph.hpp"

namespace rcspp::column_generation::testing {

/// @brief Source 0 to sink 2, through node 1 (arc costs 2 and 3) or directly (cost 7). The path
///        through node 1 covers master row 0, the direct one row 1.
struct TwoPathGraph {
        static constexpr double first_arc_cost = 2.0;
        static constexpr double second_arc_cost = 3.0;
        static constexpr double through_cost = first_arc_cost + second_arc_cost;
        static constexpr double direct_cost = 7.0;

        static std::unique_ptr<ResourceGraph<RealResource>> make() {
            auto graph = std::make_unique<ResourceGraph<RealResource>>();
            graph->add_resource<RealResource>(
                std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
            graph->add_node(0, /*source=*/true, /*sink=*/false);
            graph->add_node(1, /*source=*/false, /*sink=*/false);
            graph->add_node(2, /*source=*/false, /*sink=*/true);
            graph->add_arc<RealResource>(std::make_tuple(first_arc_cost),
                                         0,
                                         1,
                                         first_arc_cost,
                                         {Row{.index = 0, .coefficient = 1.0}});
            graph->add_arc<RealResource>(std::make_tuple(second_arc_cost), 1, 2, second_arc_cost);
            graph->add_arc<RealResource>(std::make_tuple(direct_cost),
                                         0,
                                         2,
                                         direct_cost,
                                         {Row{.index = 1, .coefficient = 1.0}});
            return graph;
        }
};

using RoutingGraph = ResourceGraph<RealResource, SizeTBitsetResource>;

/// @brief Vehicle routes as a pricing graph. The depot is the source 0 and the sink n + 1, the
///        customers are the nodes 1 to n. Resources: the cost, the load (at most @p capacity)
///        and the customers visited, so that a route visits each one at most once. An arc into
///        customer j covers master row j − 1.
///
/// @param capacity  The vehicles' capacity.
/// @param demands   The demand of customers 1 to n.
/// @param distances Between the places 0 to n, the depot being 0; the cost of the arcs.
inline std::unique_ptr<RoutingGraph> routing_graph(
    double capacity, const std::vector<double>& demands,
    const std::vector<std::vector<double>>& distances) {
    const size_t customers = demands.size();
    const size_t sink = customers + 1;

    auto graph = std::make_unique<RoutingGraph>();
    presets::add_cost_resource<RealResource>(*graph);
    presets::add_capacity_resource<RealResource>(*graph, capacity);
    std::vector<size_t> nodes(sink + 1);
    std::iota(nodes.begin(), nodes.end(), size_t{0});
    presets::add_elementary_resource<SizeTBitsetResource>(*graph, nodes);

    graph->add_node(0, /*source=*/true, /*sink=*/false);
    for (size_t customer = 1; customer <= customers; ++customer) {
        graph->add_node(customer);
    }
    graph->add_node(sink, /*source=*/false, /*sink=*/true);

    // The visited set ignores its arc values: it reads the arc's endpoints.
    const std::set<size_t> none;
    for (size_t from = 0; from <= customers; ++from) {
        for (size_t to = 1; to <= sink; ++to) {
            const size_t place = to == sink ? 0 : to;  // the sink is the depot again
            if (place == from) {
                continue;  // no loop, and no empty route
            }
            const double distance = distances[from][place];
            const double demand = to == sink ? 0.0 : demands[to - 1];
            std::vector<Row> rows;
            if (to != sink) {
                rows.push_back(Row{.index = to - 1, .coefficient = 1.0});
            }
            graph->add_arc<RealResource, RealResource, SizeTBitsetResource>(
                {distance, demand, none},
                from,
                to,
                distance,
                std::move(rows));
        }
    }
    return graph;
}

using StageGraph = ResourceGraph<RealResource>;
using Stage = PricingStage<RealResource>;
using Strategy = PricingStrategy<RealResource>;

/// @brief A stage that answers @p answer without searching, and records each request.
inline Stage scripted_stage(std::vector<StageRequest>* requests, bool exact,
                            SolveResult answer = {}) {
    return {[requests, answer](StageGraph& /*graph*/, const StageRequest& request) {
                requests->push_back(request);
                return answer;
            },
            exact};
}

}  // namespace rcspp::column_generation::testing
