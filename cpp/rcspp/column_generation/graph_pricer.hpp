// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/pricing_strategy.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "rcspp/utils/timer.hpp"

namespace rcspp::column_generation {

/// @brief A pricer over one @c ResourceGraph: the pricing engine as the loop calls it.
///
/// Each call writes the reduced costs into the graph (an arc's cost minus the duals of its rows,
/// in the cost component @c cost_index), then runs the strategy's stages from the first: it stops
/// at the first stage that finds a path below the threshold, or whose search proves that none
/// exists, and moves to the next stage otherwise. The columns are the paths found, each with its
/// original cost and rows (@c Solution::column).
///
/// A call proves that no other improving path exists only when an exact stage finished: status
/// @c AlgorithmStatus::COMPLETE and no memory-pressure trim, which can follow a complete status.
/// Its bound on the best reduced cost is then the best path found, or the threshold when none
/// was found; otherwise the bound is `−∞` and the status @c PricingStatus::SolverLimit.
///
/// Holds the graph by reference: the caller keeps it alive and does not solve it elsewhere
/// during a call (a solve that cannot lock the graph returns nothing, which would read as a
/// proof).
///
/// @tparam ResourceTypes The resource types of the graph.
template <typename... ResourceTypes>
class GraphPricer final : public Pricer {
    public:
        using Graph = ResourceGraph<ResourceTypes...>;
        using Strategy = PricingStrategy<ResourceTypes...>;

        /// @brief Constructs the pricer.
        ///
        /// @param graph      The pricing graph; its arcs' rows are master rows.
        /// @param strategy   The stages; one exact stage by default.
        /// @param cost_index The cost component that holds the reduced costs.
        /// @param preprocess Whether each search reduces the graph first. The engine runs the
        ///                   reduction that does not depend on costs only after the graph
        ///                   changed, and the one that does (shortest paths against the
        ///                   threshold) on every search.
        explicit GraphPricer(Graph& graph, Strategy strategy = Strategy(), size_t cost_index = 0,
                             bool preprocess = true)
            : graph_(graph),
              strategy_(std::move(strategy)),
              cost_index_(cost_index),
              preprocess_(preprocess) {}

        /// @brief Prices the duals.
        ///
        /// @throws std::logic_error when the graph has no source or no sink.
        /// @throws std::out_of_range when the cost index is not a cost component of the graph,
        ///                           or an arc's row is not a master row.
        PricingResult price(std::span<const double> duals, const PricingRequest& request) override {
            if (graph_.get_source_node_ids().empty() || graph_.get_sink_node_ids().empty()) {
                throw std::logic_error("GraphPricer: the graph has no source or no sink");
            }
            // The engine would only warn, and search the costs left from before.
            if (cost_index_ >=
                graph_.get_resource_factory().template get_num_resource_type<RealResource>()) {
                throw std::out_of_range(
                    "GraphPricer: the cost index is not a cost component of "
                    "the graph");
            }

            // The engine takes the duals as a vector: they are copied into one kept between calls.
            duals_.assign(duals.begin(), duals.end());
            graph_.update_reduced_costs(duals_, cost_index_);

            const Timer timer(true);
            strategy_.reset();
            while (true) {
                const StageRequest stage_request{
                    .upper_bound = request.reduced_cost_threshold,
                    .timeout_s = std::max(0.0, request.timeout_s - timer.elapsed_seconds()),
                    .preprocess = preprocess_,
                    .cost_index = cost_index_};
                PricingResult result =
                    to_pricing_result(strategy_.current().search(graph_, stage_request),
                                      strategy_.is_exact(),
                                      request.reduced_cost_threshold);

                // Only an exact stage that finished is optimal.
                const bool proven = result.status == PricingStatus::Optimal;
                const bool out_of_time = timer.elapsed_seconds() >= request.timeout_s;
                if (!result.columns.empty() || proven || out_of_time || !strategy_.next_stage()) {
                    return result;
                }
            }
        }

        /// @brief The largest absolute arc cost (original costs, not reduced ones).
        [[nodiscard]] double cost_scale() const override {
            double scale = 0.0;
            graph_.for_each_arc(
                [&scale](const auto& arc) { scale = std::max(scale, std::abs(arc.cost)); });
            return scale;
        }

        /// @brief The stages.
        [[nodiscard]] const Strategy& strategy() const { return strategy_; }

    private:
        // Keeps the paths below the threshold, and proves only when an exact search finished.
        static PricingResult to_pricing_result(SolveResult searched, bool exact, double threshold) {
            PricingResult result;
            result.exact_stage = exact;

            double best = threshold;
            for (Solution& solution : searched.solutions) {
                best = std::min(best, solution.cost);
                if (solution.cost < threshold) {
                    result.columns.push_back(std::move(solution));
                }
            }

            const bool finished =
                searched.status == AlgorithmStatus::COMPLETE && !searched.memory_pressure_triggered;
            if (exact && finished) {
                result.status = PricingStatus::Optimal;
                result.min_reduced_cost_bound = best;
            } else {
                result.status = PricingStatus::SolverLimit;
            }
            return result;
        }

        Graph& graph_;
        Strategy strategy_;
        size_t cost_index_;
        bool preprocess_;
        std::vector<double> duals_;
};

}  // namespace rcspp::column_generation
