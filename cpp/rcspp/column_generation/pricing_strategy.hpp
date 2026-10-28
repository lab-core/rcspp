// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/simple_dominance_algorithm.hpp"
#include "rcspp/resource/resource_graph.hpp"

namespace rcspp::column_generation {

/// @brief What a pricing stage is asked to search.
struct StageRequest {
        /// @brief Keep only the paths whose cost (the reduced cost) is below this.
        double upper_bound = 0.0;
        /// @brief Wall-clock limit in seconds; caps the stage's own limit.
        double timeout_s = std::numeric_limits<double>::infinity();
        /// @brief Whether the graph is reduced before the search.
        bool preprocess = true;
        /// @brief The cost component that holds the reduced costs.
        size_t cost_index = 0;
};

/// @brief One way of searching a pricing graph, and whether it is exact.
///
/// An exact stage that finishes proves that no path better than those it returns exists; a
/// heuristic stage, or one stopped early, proves nothing. The search is held as a function, so a
/// stage can run any algorithm of the engine (see @c make) or the user's own.
///
/// @tparam ResourceTypes The resource types of the graph.
template <typename... ResourceTypes>
class PricingStage {
    public:
        using Graph = ResourceGraph<ResourceTypes...>;
        using Search = std::function<SolveResult(Graph&, const StageRequest&)>;

        /// @brief Constructs a stage.
        ///
        /// @param search Runs the search; it must honor every field of the request.
        /// @param exact  Whether a finished search is exhaustive.
        /// @throws std::invalid_argument when @p search is empty.
        PricingStage(Search search, bool exact) : search_(std::move(search)), exact_(exact) {
            if (!search_) {
                throw std::invalid_argument("PricingStage: the search is empty");
            }
        }

        /// @brief A stage that runs one of the engine's algorithms through
        ///        @c ResourceGraph::solve.
        ///
        /// The request's time limit caps `params.timeout_s`.
        ///
        /// @tparam AlgorithmType      The algorithm template, e.g. @c SimpleDominanceAlgorithm.
        /// @tparam CostResourceType   The resource type holding the cost.
        /// @tparam LabelContainerType The per-node label container.
        /// @tparam ClockResourceType  Where a bidirectional search's halves stop; see
        ///                            @c ResourceGraph::solve.
        /// @param params The algorithm's parameters, copied for every search.
        /// @param exact  Whether the algorithm, when it finishes, is exhaustive.
        template <
            template <typename, typename> class AlgorithmType = SimpleDominanceAlgorithm,
            typename CostResourceType = RealResource,
            typename LabelContainerType = LabelList<ResourceTypeComposition<ResourceTypes...>>,
            typename ClockResourceType = CostResourceType>
        [[nodiscard]] static PricingStage make(
            AlgorithmParams<LabelContainerType> params = AlgorithmParams<LabelContainerType>(),
            bool exact = true) {
            Search search = [params = std::move(params)](Graph& graph,
                                                         const StageRequest& request) {
                AlgorithmParams<LabelContainerType> capped = params;
                capped.timeout_s = std::min(capped.timeout_s, request.timeout_s);
                return graph.template solve<AlgorithmType,
                                            CostResourceType,
                                            LabelContainerType,
                                            ClockResourceType>(request.upper_bound,
                                                               std::move(capped),
                                                               request.preprocess,
                                                               request.cost_index);
            };
            return {std::move(search), exact};
        }

        /// @brief Searches @p graph.
        SolveResult search(Graph& graph, const StageRequest& request) const {
            return search_(graph, request);
        }

        /// @brief Whether a finished search is exhaustive.
        [[nodiscard]] bool exact() const { return exact_; }

    private:
        Search search_;
        bool exact_;
};

/// @brief The stages of a pricer, from the cheapest to the most exact.
///
/// A pricer starts at the first stage, moves to the next one while a stage finds nothing, and
/// comes back to the first after a stage found columns (@c GraphPricer starts from the first on
/// every call, so its cursor shows the last stage it ran). Only an exact stage that finds nothing
/// lets the loop stop with a proof, so the last stage is usually exact. The default is one exact
/// stage: @c SimpleDominanceAlgorithm with its default parameters.
///
/// @tparam ResourceTypes The resource types of the graph.
template <typename... ResourceTypes>
class PricingStrategy {
    public:
        using Stage = PricingStage<ResourceTypes...>;

        /// @brief One exact stage, with the default algorithm and parameters.
        PricingStrategy() : PricingStrategy(std::vector<Stage>{Stage::make()}) {}

        /// @brief Constructs the strategy.
        ///
        /// @param stages The stages, from the cheapest to the most exact.
        /// @throws std::invalid_argument when @p stages is empty.
        explicit PricingStrategy(std::vector<Stage> stages) : stages_(std::move(stages)) {
            if (stages_.empty()) {
                throw std::invalid_argument("PricingStrategy: no stage");
            }
        }

        /// @brief The stage to run now.
        [[nodiscard]] const Stage& current() const { return stages_[current_]; }

        /// @brief Its position among the stages.
        [[nodiscard]] size_t current_index() const { return current_; }

        /// @brief Whether the stage to run now is exact.
        [[nodiscard]] bool is_exact() const { return current().exact(); }

        /// @brief Moves to the next stage, after the current one found nothing.
        ///
        /// @return Whether there was a next stage; at the last one, nothing changes.
        bool next_stage() {
            if (current_ + 1 == stages_.size()) {
                return false;
            }
            ++current_;
            return true;
        }

        /// @brief Comes back to the first stage, e.g. after a stage found columns.
        void reset() { current_ = 0; }

        /// @brief Every stage, in order.
        [[nodiscard]] std::span<const Stage> stages() const { return stages_; }

    private:
        std::vector<Stage> stages_;
        size_t current_ = 0;
};

}  // namespace rcspp::column_generation
