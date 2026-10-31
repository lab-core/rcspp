// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The labelling kernel's schedules, driven by hand: the order they hand labels out in, the
// per-node quota and the trim under each role, and the next phase. Then a search run step by
// step with a schedule, forward and backward.

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace schedules_test {

using Composed = ResourceTypeComposition<RealResource>;
using Entry = parts::StoredLabel<Composed>;
using Fifo = parts::FifoSchedule<Composed>;

static_assert(parts::LabelSchedule<Fifo, Composed>);

/// @brief A line 0 -> 1 -> 2 whose one resource is the cost, a pool for its labels, and a
///        context pointing at the fixture's own state.
class Fixture {
    public:
        explicit Fixture(detail::Role role = detail::Role::Single)
            : graph_(std::make_unique<ResourceGraph<RealResource>>()) {
            graph_->add_resource<RealResource>(
                std::make_unique<AdditionExtensionFunction<RealResource>>(),
                std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                std::make_unique<ValueCostFunction<RealResource>>(),
                std::make_unique<ValueDominanceFunction<RealResource>>());
            graph_->add_node(0, /*source=*/true, /*sink=*/false);
            graph_->add_node(1);
            graph_->add_node(2, /*source=*/false, /*sink=*/true);
            graph_->add_arc<RealResource>({1.0}, 0, 1, 1.0);
            graph_->add_arc<RealResource>({1.0}, 1, 2, 1.0);
            graph_->sort_nodes();
            pool_ = std::make_unique<LabelPool<Composed>>(
                std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory()));
            ctx.graph = graph_.get();
            ctx.pool = pool_.get();
            ctx.params = &params;
            ctx.quota = &quota;
            ctx.bound = &bound;
            ctx.role = role;
            ctx.abandoned = &abandoned;
        }

        /// @brief An entry for a new label at node @p node_id costing @p cost.
        Entry label(size_t node_id, double cost, bool dominated = false) {
            auto& made = pool_->get_next_label(graph_->get_node(node_id));
            made.get_resource().get_component<RealResource>(0).set_value(cost);
            made.dominated = dominated;
            return {.label = &made, .position = {}};
        }

        /// @brief The labels the pool has handed out and not had back.
        [[nodiscard]] size_t in_use() const {
            return pool_->get_nb_total_labels() - pool_->get_nb_available_labels();
        }

        /// @brief Hands out every label @p schedule has, as costs.
        template <typename ScheduleType>
        std::vector<double> drain(ScheduleType* schedule) {
            std::vector<double> costs;
            for (auto entry = schedule->next(ctx); entry.label != nullptr;
                 entry = schedule->next(ctx)) {
                costs.push_back(entry.label->get_cost());
            }
            return costs;
        }

        AlgorithmBaseParams params;
        size_t quota = std::numeric_limits<size_t>::max();
        size_t abandoned = 0;
        detail::NodeBound<Composed> bound;
        detail::SearchContext<Composed> ctx;

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        std::unique_ptr<LabelPool<Composed>> pool_;
};

/// @brief Counts the labels a schedule asks to erase from their containers.
struct CountingErase {
        size_t* erased;
        void operator()(const LabelIterator<Composed>& /*position*/) const { ++*erased; }
};

}  // namespace schedules_test

/// @brief Labels come out in the order they went in; an empty schedule hands out a null label.
TEST(FifoSchedule, HandsLabelsOutFirstInFirstOut) {
    namespace st = schedules_test;
    st::Fixture f;
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    EXPECT_EQ(fifo.next(f.ctx).label, nullptr);
    fifo.push(f.label(2, 3.0));
    fifo.push(f.label(1, 1.0));
    fifo.push(f.label(1, 2.0));
    EXPECT_EQ(fifo.size(), 3U);
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{3.0, 1.0, 2.0}));
    EXPECT_EQ(fifo.size(), 0U);
    EXPECT_EQ(fifo.next(f.ctx).label, nullptr);
}

/// @brief A label dominated while it waited is released, and the next one handed out instead.
TEST(FifoSchedule, ReleasesADominatedLabelAndGoesOn) {
    namespace st = schedules_test;
    st::Fixture f;
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    fifo.push(f.label(1, 1.0, /*dominated=*/true));
    fifo.push(f.label(1, 2.0));
    EXPECT_EQ(f.in_use(), 2U);
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{2.0}));
    EXPECT_EQ(f.in_use(), 1U);
}

/// @brief Over the per-node quota, a label is stored aside and queued again by the next phase,
///        which resets the counts.
TEST(FifoSchedule, StoresAsideWhatTheQuotaCutsUntilTheNextPhase) {
    namespace st = schedules_test;
    st::Fixture f;
    f.quota = 1;
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    fifo.push(f.label(1, 1.0));
    fifo.push(f.label(1, 2.0));
    fifo.push(f.label(2, 3.0));
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{1.0, 3.0}));
    EXPECT_EQ(fifo.size(), 0U);
    EXPECT_EQ(f.abandoned, 0U);

    fifo.prepare_next_phase(f.ctx);
    EXPECT_EQ(fifo.size(), 1U);
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{2.0}));
}

/// @brief On a bidirectional side, a label over the quota is counted and dropped: the next phase
///        has nothing to queue again.
TEST(FifoSchedule, ABidirectionalSideAbandonsAndCountsWhatTheQuotaCuts) {
    namespace st = schedules_test;
    st::Fixture f(detail::Role::BidirectionalSide);
    f.quota = 1;
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    fifo.push(f.label(1, 1.0));
    fifo.push(f.label(1, 2.0));
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{1.0}));
    EXPECT_EQ(f.abandoned, 1U);
    EXPECT_EQ(f.in_use(), 2U) << "an abandoned label stays where it is";
    fifo.prepare_next_phase(f.ctx);
    EXPECT_EQ(fifo.size(), 0U);
}

/// @brief A trim keeps the best labels (one per node here), releases the dominated excess and
///        stores the rest aside; releasing what was stored aside erases and releases it.
TEST(FifoSchedule, ATrimStoresAsideWhatItCuts) {
    namespace st = schedules_test;
    st::Fixture f;
    f.quota = 5;
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    for (const double cost : {5.0, 1.0, 4.0, 2.0}) {
        fifo.push(f.label(1, cost));
    }
    fifo.push(f.label(1, 0.5, /*dominated=*/true));
    fifo.trim(f.ctx, 1);
    EXPECT_EQ(fifo.size(), 3U);  // 1 per node, 3 nodes: the three cheapest non-dominated
    EXPECT_EQ(f.in_use(), 4U);   // the dominated one released
    f.quota = 1;                 // as memory pressure tightens it
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{1.0}));  // the quota of 1 at node 1

    size_t erased = 0;
    fifo.release_stored_aside(f.ctx, st::CountingErase{&erased});
    EXPECT_EQ(erased, 3U);  // the 5 stored aside by the trim, the 2 and 4 by the quota
    EXPECT_EQ(f.in_use(), 1U);
    fifo.prepare_next_phase(f.ctx);
    EXPECT_EQ(fifo.size(), 0U);
}

/// @brief On a bidirectional side, a trim counts the non-dominated excess instead of storing it,
///        and there is nothing stored aside to release.
TEST(FifoSchedule, ABidirectionalSideAbandonsWhatATrimCuts) {
    namespace st = schedules_test;
    st::Fixture f(detail::Role::BidirectionalSide);
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    for (const double cost : {5.0, 1.0, 4.0, 2.0}) {
        fifo.push(f.label(1, cost));
    }
    fifo.push(f.label(1, 0.5, /*dominated=*/true));
    fifo.trim(f.ctx, 1);
    EXPECT_EQ(fifo.size(), 3U);
    EXPECT_EQ(f.abandoned, 1U);  // the 5
    EXPECT_EQ(f.in_use(), 4U);
    size_t erased = 0;
    fifo.release_stored_aside(f.ctx, st::CountingErase{&erased});
    EXPECT_EQ(erased, 0U);
}

/// @brief clear forgets every label and resets the counts.
TEST(FifoSchedule, ClearForgetsEverything) {
    namespace st = schedules_test;
    st::Fixture f;
    f.quota = 1;
    st::Fifo fifo;
    fifo.initialize(f.ctx);
    fifo.push(f.label(1, 1.0));
    fifo.push(f.label(1, 2.0));
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{1.0}));
    fifo.push(f.label(1, 3.0));
    fifo.clear();
    EXPECT_EQ(fifo.size(), 0U);
    fifo.prepare_next_phase(f.ctx);
    EXPECT_EQ(fifo.size(), 0U) << "nothing stored aside survives";
    fifo.push(f.label(1, 4.0));
    EXPECT_EQ(f.drain(&fifo), (std::vector<double>{4.0})) << "the count was reset";
}

/// @brief The per-node quota counts the labels taken at each node until the next phase restarts
///        it.
TEST(PerNodeQuota, CountsTheLabelsTakenAtEachNode) {
    parts::PerNodeQuota quota;
    quota.reset(2);
    EXPECT_TRUE(quota.may_take(0, 2));
    EXPECT_TRUE(quota.may_take(0, 2));
    EXPECT_FALSE(quota.may_take(0, 2));
    EXPECT_TRUE(quota.may_take(1, 2)) << "each node counts on its own";
    EXPECT_FALSE(quota.may_take(1, 0));
    quota.restart();
    EXPECT_TRUE(quota.may_take(0, 1));
}

/// @brief The bound a prune reads, and the one an order reads: -infinity counts as 0 for an
///        order; no bound is 0 everywhere.
TEST(NodeBound, ReadsTheBoundForAPruneAndForAnOrder) {
    namespace st = schedules_test;
    st::Fixture f;
    const auto entry = f.label(1, 1.0);
    detail::NodeBound<st::Composed> bound;
    EXPECT_EQ(bound(*entry.label), 0.0);
    EXPECT_EQ(bound.for_order(*entry.label), 0.0);
    bound.by_position = {0.0, 2.5, 0.0};
    EXPECT_EQ(bound(*entry.label), 2.5);
    EXPECT_EQ(bound.for_order(*entry.label), 2.5);
    bound.by_position[1] = -std::numeric_limits<double>::infinity();
    EXPECT_EQ(bound(*entry.label), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(bound.for_order(*entry.label), 0.0);
}

namespace search_step_test {

using Composed = ResourceTypeComposition<RealResource>;
using List = LabelList<Composed>;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// @brief Source 0, sink 3. Arcs: 0 -> 3 (5), 0 -> 1 (1), 1 -> 2 (0), 2 -> 3 (1), 1 -> 3 (3), with
///        ids 0 to 4. The optimum, 0 -> 1 -> 2 -> 3, costs 2; the others cost 5 and 4.
inline std::unique_ptr<ResourceGraph<RealResource>> four_nodes() {
    auto graph = std::make_unique<ResourceGraph<RealResource>>();
    graph->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                      std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                      std::make_unique<ValueCostFunction<RealResource>>(),
                                      std::make_unique<ValueDominanceFunction<RealResource>>());
    graph->add_node(0, /*source=*/true, /*sink=*/false);
    graph->add_node(1);
    graph->add_node(2);
    graph->add_node(3, /*source=*/false, /*sink=*/true);
    graph->add_arc<RealResource>({5.0}, 0, 3, 5.0);
    graph->add_arc<RealResource>({1.0}, 0, 1, 1.0);
    graph->add_arc<RealResource>({0.0}, 1, 2, 0.0);
    graph->add_arc<RealResource>({1.0}, 2, 3, 1.0);
    graph->add_arc<RealResource>({3.0}, 1, 3, 3.0);
    graph->sort_nodes();
    graph->build_csr();
    return graph;
}

/// @brief One search in @p Direction with a first-in, first-out schedule, and what an algorithm
///        would give it: the context, a record callable and a count callable.
template <typename Direction>
class Driven {
    public:
        using Container = detail::directed_container_t<List, Direction>;
        using SearchType = detail::Search<Composed, Direction, Container,
                                          detail::Preset<parts::FifoSchedule<Composed>>>;

        explicit Driven(const AlgorithmBaseParams& params)
            : graph_(four_nodes()),
              pool_(std::make_unique<LabelFactory<Composed>>(&graph_->get_resource_factory())),
              params_(params) {}

        /// @brief Runs the search to its end, or until a step says a budget is spent.
        ///
        /// @return Whether a budget was spent.
        bool run() {
            search.reset(graph_->get_number_of_nodes(),
                         detail::make_directed_container<Direction>(List{}));
            ctx = {.graph = graph_.get(),
                   .pool = &pool_,
                   .params = &params_,
                   .upper_bound = kInfinity,
                   .incumbent = &incumbent_,
                   .quota = &quota_,
                   .solutions = &solutions_,
                   .bound = &search.bound};
            search.schedule.initialize(ctx);
            search.create_start_labels(*graph_, &pool_);
            const auto record = [this](const Label<Composed>& label) {
                std::vector<size_t> arc_ids;
                if constexpr (Direction::backward) {
                    append_suffix_arc_ids(label, &arc_ids);
                } else {
                    arc_ids = prefix_arc_ids(label);
                }
                solutions_.insert(Solution(label.get_cost(), {}, arc_ids));
                recorded.push_back(label.get_cost());
            };
            const auto count = [this](parts::ExtensionOutcome /*outcome*/) { ++extensions; };
            while (search.schedule.size() > 0) {
                if (detail::step(search, ctx, record, count)) {
                    return true;
                }
            }
            return false;
        }

        /// @brief The costs of the labels left at the end nodes of @p Direction.
        [[nodiscard]] std::vector<double> end_node_costs() const {
            std::vector<double> costs;
            for (const auto* label : search.labels_at_end_nodes(*graph_)) {
                costs.push_back(label->get_cost());
            }
            return costs;
        }

        /// @brief The costs of the labels left at node @p node_id.
        [[nodiscard]] std::vector<double> costs_at(size_t node_id) const {
            std::vector<double> costs;
            for (const auto* label : search.labels.labels_at(graph_->get_node(node_id)->pos())) {
                costs.push_back(label->get_cost());
            }
            return costs;
        }

        [[nodiscard]] const std::unordered_set<Solution>& solutions() const { return solutions_; }
        [[nodiscard]] bool ref_counts_consistent() const {
            return pool_.check_ref_count_consistency();
        }

        SearchType search;
        detail::SearchContext<Composed> ctx;
        std::vector<double> recorded;
        size_t extensions = 0;

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        LabelPool<Composed> pool_;
        AlgorithmBaseParams params_;
        double incumbent_ = kInfinity;
        size_t quota_ = std::numeric_limits<size_t>::max();
        std::unordered_set<Solution> solutions_;
};

}  // namespace search_step_test

/// @brief Run step by step, a forward search keeps the non-dominated labels at each node, records
///        nothing during the search, and extends every label once along each of its arcs.
TEST(Search, AForwardSearchKeepsTheNonDominatedLabels) {
    namespace sst = search_step_test;
    sst::Driven<ForwardDirection> driven{AlgorithmBaseParams{}};
    EXPECT_FALSE(driven.run());
    EXPECT_EQ(driven.end_node_costs(), (std::vector<double>{2.0}));
    EXPECT_EQ(driven.costs_at(1), (std::vector<double>{1.0}));
    EXPECT_EQ(driven.costs_at(2), (std::vector<double>{1.0}));
    EXPECT_TRUE(driven.recorded.empty());
    EXPECT_EQ(driven.extensions, 5U);   // 2 arcs out of 0, 2 out of 1, 1 out of 2
    EXPECT_EQ(driven.search.work, 5U);  // the labels at the sink are not extended
    EXPECT_TRUE(driven.ref_counts_consistent());
}

/// @brief With dominated solutions returned, each end-node label is recorded when it is taken,
///        the cheapest among them; a step says the budget is spent once max_solutions are
///        recorded.
TEST(Search, EndNodeLabelsAreRecordedWhenTakenUntilTheBudgetIsSpent) {
    namespace sst = search_step_test;
    AlgorithmBaseParams params;
    params.return_dominated_solutions = true;
    params.max_solutions = 1000;
    sst::Driven<ForwardDirection> driven{params};
    EXPECT_FALSE(driven.run());
    ASSERT_GE(driven.recorded.size(), 2U);
    EXPECT_EQ(*std::ranges::min_element(driven.recorded), 2.0);
    EXPECT_EQ(driven.solutions().size(), driven.recorded.size());
    EXPECT_TRUE(driven.solutions().contains(Solution(2.0, {}, {1, 2, 3})));

    params.max_solutions = 1;
    sst::Driven<ForwardDirection> stopped{params};
    EXPECT_TRUE(stopped.run());
    EXPECT_EQ(stopped.recorded.size(), 1U);
    EXPECT_GT(stopped.search.schedule.size(), 0U) << "the stop leaves labels to take";
    EXPECT_TRUE(stopped.ref_counts_consistent());
}

/// @brief Run step by step backward, the search ends at the source with the optimum, whose path
///        reads from the source to the sink.
TEST(Search, ABackwardSearchEndsAtTheSourceWithTheOptimum) {
    namespace sst = search_step_test;
    AlgorithmBaseParams params;
    params.return_dominated_solutions = true;
    params.max_solutions = 1000;
    sst::Driven<BackwardDirection> driven{params};
    EXPECT_FALSE(driven.run());
    EXPECT_EQ(driven.end_node_costs(), (std::vector<double>{2.0}));
    ASSERT_FALSE(driven.recorded.empty());
    EXPECT_EQ(*std::ranges::min_element(driven.recorded), 2.0);
    EXPECT_TRUE(driven.solutions().contains(Solution(2.0, {}, {1, 2, 3})));
    EXPECT_TRUE(driven.ref_counts_consistent());
}

/// @brief One Bellman-Ford run over each arc's share of the cost fills the bound by node
///        position: to the sinks for a forward search, from the sources for a backward one; a
///        negative-cost cycle leaves minus infinity everywhere.
TEST(CompletionBounds, FillTheBoundFromOneBellmanFordRun) {
    namespace sst = search_step_test;
    auto graph = sst::four_nodes();
    EXPECT_EQ(detail::completion_bounds<ForwardDirection>(*graph).by_position,
              (std::vector<double>{2.0, 1.0, 1.0, 0.0}));
    EXPECT_EQ(detail::completion_bounds<BackwardDirection>(*graph).by_position,
              (std::vector<double>{0.0, 1.0, 1.0, 2.0}));

    auto cyclic = std::make_unique<ResourceGraph<RealResource>>();
    cyclic->add_resource<RealResource>(std::make_unique<AdditionExtensionFunction<RealResource>>(),
                                       std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
                                       std::make_unique<ValueCostFunction<RealResource>>(),
                                       std::make_unique<ValueDominanceFunction<RealResource>>());
    cyclic->add_node(0, /*source=*/true, /*sink=*/false);
    cyclic->add_node(1);
    cyclic->add_node(2);
    cyclic->add_node(3, /*source=*/false, /*sink=*/true);
    cyclic->add_arc<RealResource>({0.0}, 0, 1, 0.0);
    cyclic->add_arc<RealResource>({-1.0}, 1, 2, -1.0);
    cyclic->add_arc<RealResource>({-1.0}, 2, 1, -1.0);
    cyclic->add_arc<RealResource>({0.0}, 2, 3, 0.0);
    cyclic->sort_nodes();
    EXPECT_EQ(detail::completion_bounds<ForwardDirection>(*cyclic).by_position,
              (std::vector<double>(4, -std::numeric_limits<double>::infinity())));
}
