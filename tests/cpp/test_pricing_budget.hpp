// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The escalation rule a column-generation loop applies to its pricing budget
// (examples/cpp/vrp/pricing_budget.hpp): when a timed-out solve is priced again, and with what.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/pricing_budget.hpp"

namespace pricing_budget_test {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// @brief Replays scripted outcomes, recording the budget each attempt was given.
struct ScriptedPricer {
        std::vector<PricingAttempt> outcomes;
        std::vector<double> budgets;

        PricingAttempt operator()(double seconds) {
            budgets.push_back(seconds);
            const size_t i = std::min(budgets.size() - 1, outcomes.size() - 1);
            return outcomes[i];
        }
};

constexpr PricingAttempt kTimedOut{.improving = false, .proof = false, .timed_out = true};
constexpr PricingAttempt kProof{.improving = false, .proof = true, .timed_out = false};
constexpr PricingAttempt kImproving{.improving = true, .proof = false, .timed_out = true};

}  // namespace pricing_budget_test

/// @brief With no budget set, a timed-out solve is not repeated: the caller's own timeout rules.
TEST(PricingBudget, NoBudgetMeansOneAttempt) {
    namespace pb = pricing_budget_test;
    const PricingBudget budget;
    double current = budget.seconds;
    pb::ScriptedPricer pricer{.outcomes = {pb::kTimedOut}};
    const auto [last, escalations] = price_with_escalation(budget, &current, pricer);
    EXPECT_EQ(escalations, 0U);
    ASSERT_EQ(pricer.budgets.size(), 1U);
    EXPECT_EQ(pricer.budgets.front(), pb::kInf);
    EXPECT_EQ(current, pb::kInf);
    EXPECT_TRUE(last.timed_out);
}

/// @brief A truncated solve that found a column needs no second attempt.
TEST(PricingBudget, AnImprovingColumnEndsPricing) {
    namespace pb = pricing_budget_test;
    const PricingBudget budget{.seconds = 0.05};
    double current = budget.seconds;
    pb::ScriptedPricer pricer{.outcomes = {pb::kImproving}};
    const auto [last, escalations] = price_with_escalation(budget, &current, pricer);
    EXPECT_EQ(escalations, 0U);
    EXPECT_EQ(pricer.budgets.size(), 1U);
    EXPECT_TRUE(last.improving);
    EXPECT_DOUBLE_EQ(current, 0.05);
}

/// @brief Two empty timeouts, then a complete solve: the budget grows tenfold twice.
TEST(PricingBudget, TimeoutsEscalateUntilAProof) {
    namespace pb = pricing_budget_test;
    const PricingBudget budget{.seconds = 0.05};
    double current = budget.seconds;
    pb::ScriptedPricer pricer{.outcomes = {pb::kTimedOut, pb::kTimedOut, pb::kProof}};
    const auto [last, escalations] = price_with_escalation(budget, &current, pricer);
    EXPECT_EQ(escalations, 2U);
    ASSERT_EQ(pricer.budgets.size(), 3U);
    EXPECT_DOUBLE_EQ(pricer.budgets[0], 0.05);
    EXPECT_DOUBLE_EQ(pricer.budgets[1], 0.5);
    EXPECT_DOUBLE_EQ(pricer.budgets[2], 5.0);
    EXPECT_DOUBLE_EQ(current, 5.0);
    EXPECT_TRUE(last.proof);
}

/// @brief The attempt at the cap is the last, whatever it finds.
TEST(PricingBudget, TheCapIsTheLastAttempt) {
    namespace pb = pricing_budget_test;
    const PricingBudget budget{.seconds = 0.05, .max_seconds = 1.0};
    double current = budget.seconds;
    pb::ScriptedPricer pricer{.outcomes = {pb::kTimedOut}};
    const auto [last, escalations] = price_with_escalation(budget, &current, pricer);
    EXPECT_EQ(escalations, 2U);
    ASSERT_EQ(pricer.budgets.size(), 3U);
    EXPECT_DOUBLE_EQ(pricer.budgets[1], 0.5);
    EXPECT_DOUBLE_EQ(pricer.budgets[2], 1.0);
    EXPECT_TRUE(last.timed_out);
}

/// @brief A non-proof that more time cannot fix -- a truncated join, a memory trim -- is not
///        retried, even with no cap: retrying it would never end.
TEST(PricingBudget, OnlyATimeoutEscalates) {
    namespace pb = pricing_budget_test;
    const PricingBudget budget{.seconds = 0.05};
    double current = budget.seconds;
    const PricingAttempt truncated_join{.improving = false, .proof = false, .timed_out = false};
    pb::ScriptedPricer pricer{.outcomes = {truncated_join}};
    const auto [last, escalations] = price_with_escalation(budget, &current, pricer);
    EXPECT_EQ(escalations, 0U);
    EXPECT_EQ(pricer.budgets.size(), 1U);
    EXPECT_FALSE(last.proof);
}

/// @brief Out-of-range fields are refused, naming the field.
TEST(PricingBudget, CheckRefusesBadFields) {
    EXPECT_THROW((PricingBudget{.seconds = 0.0}.check()), std::invalid_argument);
    EXPECT_THROW((PricingBudget{.seconds = 0.05, .growth = 1.0}.check()), std::invalid_argument);
    EXPECT_THROW((PricingBudget{.seconds = 0.05, .growth = pricing_budget_test::kInf}.check()),
                 std::invalid_argument);
    EXPECT_THROW((PricingBudget{.seconds = 0.05, .max_seconds = 0.01}.check()),
                 std::invalid_argument);
    EXPECT_NO_THROW((PricingBudget{.seconds = 0.05}.check()));
}

/// @brief `grown` never passes the cap, and a budget at the cap cannot grow.
TEST(PricingBudget, GrownStopsAtTheCap) {
    const PricingBudget budget{.seconds = 0.05, .max_seconds = 2.0};
    EXPECT_DOUBLE_EQ(budget.grown(0.05), 0.5);
    EXPECT_DOUBLE_EQ(budget.grown(0.5), 2.0);
    EXPECT_DOUBLE_EQ(budget.grown(2.0), 2.0);
    EXPECT_TRUE(budget.can_grow(0.5));
    EXPECT_FALSE(budget.can_grow(2.0));
    EXPECT_FALSE(PricingBudget{}.can_grow(1.0)) << "no budget, nothing to grow";
}

/// @brief Only an exhaustive, untrimmed, untruncated solve proves there is no column.
TEST(PricingBudget, ProvesNoColumnNeedsAllThree) {
    rcspp::SolveResult result;
    result.status = rcspp::AlgorithmStatus::COMPLETE;
    EXPECT_TRUE(proves_no_column(result));
    result.bidirectional = rcspp::BidirectionalStats{.join_truncated = true};
    EXPECT_FALSE(proves_no_column(result));
    result.bidirectional.reset();
    result.memory_pressure_triggered = true;
    EXPECT_FALSE(proves_no_column(result));
    result.memory_pressure_triggered = false;
    result.status = rcspp::AlgorithmStatus::TIMEOUT;
    EXPECT_FALSE(proves_no_column(result));
}
