// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Small pricing graphs and pricing stages for the column generation tests.

#include <memory>
#include <tuple>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/column_generation/pricing_strategy.hpp"
#include "rcspp/graph/row.hpp"
#include "rcspp/resource/concrete/functions/cost/value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/extension/addition_extension_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
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
