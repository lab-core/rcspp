// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The labelling kernel's take rule, branch by branch, on hand-built labels: what it answers for a
// label taken from a schedule, and in which order it checks.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <unordered_set>
#include <vector>

#include "rcspp/rcspp.hpp"

using namespace rcspp;  // NOLINT(google-build-using-namespace)

namespace take_rule_test {

using Composed = ResourceTypeComposition<RealResource>;
using detail::Decision;
using detail::Verdict;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// @brief A retention type that only says whether it collects at take.
template <bool CollectsAtTake>
struct Retention {
        static constexpr bool collects_at_take = CollectsAtTake;
};

/// @brief What the take rule reads of a search: its retention type.
template <bool CollectsAtTake = false>
struct Search {
        using retention_type = Retention<CollectsAtTake>;
};

/// @brief A line 0 -> 1 -> 2 whose one resource is the cost, a pool for its labels, and a context
///        pointing at the fixture's own state.
class Fixture {
    public:
        Fixture() : graph_(std::make_unique<ResourceGraph<RealResource>>()), pool_(nullptr) {
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
            ctx.params = &params;
            ctx.incumbent = &incumbent;
            ctx.quota = &quota;
            ctx.solutions = &solutions;
            ctx.bound = &bound;
        }

        /// @brief A label at node @p node_id whose resource (its cost) is @p cost.
        Label<Composed>& label(size_t node_id, double cost) {
            auto& made = pool_->get_next_label(graph_->get_node(node_id));
            made.get_resource().get_component<RealResource>(0).set_value(cost);
            made.dominated = false;
            return made;
        }

        /// @brief The position of node @p node_id.
        [[nodiscard]] size_t pos(size_t node_id) const { return graph_->get_node(node_id)->pos(); }

        AlgorithmBaseParams params;
        double incumbent = kInfinity;
        size_t quota = 2;
        std::unordered_set<Solution> solutions;
        detail::NodeBound<Composed> bound;
        detail::SearchContext<Composed> ctx;

    private:
        std::unique_ptr<ResourceGraph<RealResource>> graph_;
        std::unique_ptr<LabelPool<Composed>> pool_;
};

using Forward = detail::TakeRule<Composed, ForwardDirection>;
using Backward = detail::TakeRule<Composed, BackwardDirection>;

/// @brief Whether @p decision is @p verdict, and records iff @p record.
inline ::testing::AssertionResult is(const Decision& decision, Verdict verdict,
                                     bool record = false) {
    if (decision.verdict == verdict && decision.record == record) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << "verdict " << static_cast<int>(decision.verdict) << ", record " << decision.record;
}

}  // namespace take_rule_test

/// @brief A label dominated while it waited is released.
TEST(TakeRule, ADominatedLabelIsReleased) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    auto& label = f.label(1, 3.0);
    label.dominated = true;
    EXPECT_TRUE(trt::is(trt::Forward::live(label, trt::Search<>{}, f.ctx), trt::Verdict::Release));
}

/// @brief The rule applies no quota: every schedule caps it. A label is extended however many
///        were taken at its node.
TEST(TakeRule, TheRuleAppliesNoQuota) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    f.quota = 0;
    EXPECT_TRUE(
        trt::is(trt::Forward::live(f.label(1, 3.0), trt::Search<>{}, f.ctx), trt::Verdict::Extend));
}

/// @brief With pruning on, a label at or above the incumbent is erased and released; a
///        bidirectional side adds its bound, a single-direction search does not.
TEST(TakeRule, ALabelAtOrAboveTheIncumbentIsPrunedWhenAsked) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    const trt::Search<> search;
    f.bound.by_position.assign(3, 0.0);
    f.bound.by_position.at(f.pos(1)) = 2.0;
    f.incumbent = 4.0;
    auto& label = f.label(1, 3.0);
    EXPECT_TRUE(trt::is(trt::Forward::live(label, search, f.ctx), trt::Verdict::Extend))
        << "pruning off";

    f.params.prune_by_incumbent = true;
    EXPECT_TRUE(trt::is(trt::Forward::live(label, search, f.ctx), trt::Verdict::Extend))
        << "3 < 4 on the cost alone";
    EXPECT_TRUE(
        trt::is(trt::Forward::live(f.label(1, 4.0), search, f.ctx), trt::Verdict::EraseAndRelease))
        << "4 >= 4";
    f.ctx.role = detail::Role::BidirectionalSide;
    EXPECT_TRUE(trt::is(trt::Forward::live(label, search, f.ctx), trt::Verdict::EraseAndRelease))
        << "3 + 2 >= 4 with the bound";
}

/// @brief An end-node label lowers the incumbent when below the upper bound, stays stored, and is
///        recorded at take only when the search returns dominated solutions or collects at take.
TEST(TakeRule, AnEndNodeLabelLowersTheIncumbentAndIsKept) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    const trt::Search<> search;
    f.ctx.upper_bound = 10.0;
    f.incumbent = 10.0;
    EXPECT_TRUE(
        trt::is(trt::Forward::settled(f.label(2, 12.0), search, f.ctx), trt::Verdict::Keep));
    EXPECT_EQ(f.incumbent, 10.0) << "above the upper bound: no new incumbent";

    EXPECT_TRUE(trt::is(trt::Forward::settled(f.label(2, 6.0), search, f.ctx), trt::Verdict::Keep));
    EXPECT_EQ(f.incumbent, 6.0);
    EXPECT_TRUE(trt::is(trt::Forward::settled(f.label(2, 8.0), search, f.ctx), trt::Verdict::Keep));
    EXPECT_EQ(f.incumbent, 6.0) << "the incumbent only falls";

    EXPECT_TRUE(trt::is(trt::Forward::settled(f.label(2, 7.0), trt::Search<true>{}, f.ctx),
                        trt::Verdict::Keep,
                        /*record=*/true))
        << "a retention that collects at take";
    f.params.return_dominated_solutions = true;
    EXPECT_TRUE(trt::is(trt::Forward::settled(f.label(2, 7.0), search, f.ctx),
                        trt::Verdict::Keep,
                        /*record=*/true));

    // Going backward, the end nodes are the sources.
    EXPECT_TRUE(
        trt::is(trt::Backward::settled(f.label(2, 7.0), search, f.ctx), trt::Verdict::Extend));
}

/// @brief A label of infinite cost short of an end node is erased and released.
TEST(TakeRule, AnInfiniteCostLabelIsErased) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    const trt::Search<> search;
    EXPECT_TRUE(trt::is(trt::Forward::settled(f.label(1, trt::kInfinity), search, f.ctx),
                        trt::Verdict::EraseAndRelease));
    EXPECT_TRUE(
        trt::is(trt::Forward::settled(f.label(1, 3.0), search, f.ctx), trt::Verdict::Extend));
}

/// @brief A label past the half-way point stays stored, not extended: forward above H, backward
///        below it; with the point off, nothing is past it.
TEST(TakeRule, ALabelPastTheHalfWayPointIsKeptStored) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    const trt::Search<> search;
    const detail::HalfWay<trt::Composed> half_way(  // H = 5, read on the cost
        5.0,
        0,
        &critical_value<RealResource, trt::Composed>);
    f.ctx.half_way = &half_way;
    EXPECT_TRUE(trt::is(trt::Forward::settled(f.label(1, 6.0), search, f.ctx), trt::Verdict::Keep));
    EXPECT_TRUE(
        trt::is(trt::Forward::settled(f.label(1, 4.0), search, f.ctx), trt::Verdict::Extend));
    EXPECT_TRUE(
        trt::is(trt::Backward::settled(f.label(1, 4.0), search, f.ctx), trt::Verdict::Keep));
    EXPECT_TRUE(
        trt::is(trt::Backward::settled(f.label(1, 6.0), search, f.ctx), trt::Verdict::Extend));

    auto off = half_way;
    off.turn_off();
    f.ctx.half_way = &off;
    EXPECT_TRUE(
        trt::is(trt::Forward::settled(f.label(1, 6.0), search, f.ctx), trt::Verdict::Extend));
}

/// @brief The live checks come first: a label both dominated and at an end node is released by
///        them, so the settled checks, which would record it, never run for it.
TEST(TakeRule, TheLiveChecksComeFirst) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    const trt::Search<> search;
    f.params.return_dominated_solutions = true;
    auto& label = f.label(2, 3.0);
    label.dominated = true;
    const trt::Decision live = trt::Forward::live(label, search, f.ctx);
    EXPECT_TRUE(trt::is(live, trt::Verdict::Release));
    EXPECT_FALSE(live.record);
    label.dominated = false;
    EXPECT_TRUE(trt::is(trt::Forward::settled(label, search, f.ctx),
                        trt::Verdict::Keep,
                        /*record=*/true));
}

/// @brief The solve's budget is spent once the solutions recorded reach max_solutions.
TEST(TakeRule, TheBudgetIsSpentAtMaxSolutions) {
    namespace trt = take_rule_test;
    trt::Fixture f;
    f.params.max_solutions = 2;
    EXPECT_FALSE(f.ctx.budget_spent());
    f.solutions.insert(Solution(1.0, {}, {0}));
    EXPECT_FALSE(f.ctx.budget_spent());
    f.solutions.insert(Solution(2.0, {}, {1}));
    EXPECT_TRUE(f.ctx.budget_spent());
}
