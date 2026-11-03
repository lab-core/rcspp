// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Parity of the heuristics with their frozen copies: Greedy, the tabu search, the improving tabu
// search and diversification. Its own translation unit: the copies double the heuristics'
// instantiations.

#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "reference/diversification_search.hpp"
#include "reference/greedy.hpp"
#include "reference/improving_tabu_search.hpp"
#include "reference/tabu_search.hpp"
#include "util/dive_parity.hpp"

namespace {

namespace gl = golden_labelling;
namespace ref = rcspp::reference;
namespace strategy = rcspp::strategy;

/// @brief The frozen Greedy, as a strategy.
struct FrozenGreedy {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = ref::GreedyAlgorithm<ResourceType, LabelContainerType>;
};
/// @brief The frozen tabu search, as a strategy.
struct FrozenTabu {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = ref::TabuSearchAlgorithm<ResourceType, LabelContainerType>;
};
/// @brief The frozen improving tabu search, as a strategy.
struct FrozenImprovingTabu {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = ref::ImprovingTabuSearch<ResourceType, LabelContainerType>;
};
/// @brief Whether a solve of the frozen Greedy ran out of its iterations since the flag was last
///        cleared.
inline bool& frozen_greedy_ran_out() {
    static bool ran_out = false;
    return ran_out;
}

/// @brief The frozen Greedy, raising @ref frozen_greedy_ran_out when a solve ends
///        @c Truncated: when its iterations ran out.
template <typename ResourceType, typename LabelContainerType>
class WatchedGreedy : public ref::GreedyAlgorithm<ResourceType, LabelContainerType> {
    public:
        using ref::GreedyAlgorithm<ResourceType, LabelContainerType>::GreedyAlgorithm;

        rcspp::SolveResult solve(const rcspp::Graph<ResourceType>* graph,
                                 double cost_upper_bound) override {
            auto result =
                ref::GreedyAlgorithm<ResourceType, LabelContainerType>::solve(graph,
                                                                              cost_upper_bound);
            if (result.status == rcspp::AlgorithmStatus::Truncated) {
                frozen_greedy_ran_out() = true;
            }
            return result;
        }
};

/// @brief The frozen Greedy, watched, as a strategy.
struct FrozenWatchedGreedy {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm = WatchedGreedy<ResourceType, LabelContainerType>;
};

/// @brief The frozen diversification over the frozen Greedy, watched, as a strategy.
struct FrozenDiversification {
        template <typename ResourceType, typename LabelContainerType>
        using algorithm =
            ref::DiversificationSearch<ResourceType, LabelContainerType, FrozenWatchedGreedy>;
};

/// @brief The golden instance of seed 1 at 11 nodes, with the draw log appending to @p log.
std::unique_ptr<dive_parity::GraphType> small_graph(
    const std::shared_ptr<std::vector<dive_parity::Draw>>& log) {
    return dive_parity::build(gl::generate({1, 11, 1}), log);
}

}  // namespace

/// @brief The draw log sees every label a dive draws but the source labels, and the arc each was
///        extended along leaves the label it extends.
TEST(DiveParity, TheDrawLogSeesEveryLabelDrawn) {
    auto log = std::make_shared<std::vector<dive_parity::Draw>>();
    auto graph = small_graph(log);
    rcspp::AlgorithmParams<parity::List> params;
    params.release_after_solve = false;
    auto algorithm = graph->create_algorithm<FrozenGreedy>(params);
    const auto result = graph->solve(algorithm.get(), parity::kInf, false);
    ASSERT_FALSE(result.solutions.empty());
    const auto& pool = algorithm->get_label_pool();
    const auto drawn =
        static_cast<size_t>(pool.get_nb_created_labels() + pool.get_nb_reused_labels());
    EXPECT_EQ(log->size() + graph->get_source_node_ids().size(), drawn);
    for (size_t i = 0; i < log->size(); ++i) {
        const auto& draw = (*log)[i];
        ASSERT_LT(draw.parent, i + 1) << "draw " << i + 1 << " extends a later label";
        const auto* arc = graph->get_arc(draw.arc_id);
        ASSERT_NE(arc, nullptr);
        if (draw.parent == 0) {
            EXPECT_TRUE(arc->origin->source) << "draw " << i + 1;
        } else {
            const auto* parent_arc = graph->get_arc((*log)[draw.parent - 1].arc_id);
            EXPECT_EQ(parent_arc->destination, arc->origin) << "draw " << i + 1;
        }
    }
}

/// @brief Two different searches leave different observations: the comparison is not vacuous.
TEST(DiveParity, TellsTwoSearchesApart) {
    const auto cases = dive_parity::cases();
    ASSERT_FALSE(cases.empty());
    const dive_parity::Setting exact{gl::heuristic_variants().front()};
    const auto greedy = dive_parity::observe<FrozenGreedy>(cases.front(), exact, true, false);
    const auto tabu = dive_parity::observe<FrozenTabu>(cases.front(), exact, true, false);
    EXPECT_FALSE(dive_parity::same(greedy, tabu));
}

/// @brief Greedy, a depth-first labelling search, draws the frozen Greedy's labels, in its order,
///        and returns its solutions, wherever no iteration budget applies: it counts labels taken,
///        where the frozen Greedy counted dives.
TEST(DiveParity, GreedyMatchesItsFrozenCopy) {
    EXPECT_GT(
        (dive_parity::sweep<FrozenGreedy, strategy::Greedy>(false, /*compare_budgets=*/false)),
        0U);
}

TEST(DiveParity, TabuSearchMatchesItsFrozenCopy) {
    dive_parity::sweep<FrozenTabu, strategy::Tabu>(true);
}

TEST(DiveParity, ImprovingTabuSearchMatchesItsFrozenCopy) {
    dive_parity::sweep<FrozenImprovingTabu, strategy::ImprovingTabu>(true);
}

/// @brief Diversification over Greedy matches its frozen copy wherever the frozen inner Greedy
///        never ran out of its iterations.
///
/// The frozen copy gives its inner Greedy 20 iterations, which counted dives; diversification now
/// gives it 1000, which count labels taken. Where 20 dives run out, the two inner searches stop at
/// different places: those runs are left out, and counted.
TEST(DiveParity, DiversificationMatchesItsFrozenCopy) {
    size_t left_out = 0;
    const size_t compared =
        dive_parity::sweep<FrozenDiversification, strategy::Diversification<>>(true, true, [&] {
            const bool ran_out = frozen_greedy_ran_out();
            frozen_greedy_ran_out() = false;
            left_out += ran_out ? 1 : 0;
            return !ran_out;
        });
    RecordProperty("compared", static_cast<int>(compared));
    RecordProperty("left_out", static_cast<int>(left_out));
    EXPECT_GT(compared, left_out) << left_out << " runs left out, " << compared << " compared";
}
