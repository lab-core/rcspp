// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// A per-solve pricing budget for column generation, and the rule that keeps it safe: a solve that
// timed out with no improving column is priced again, on the same duals, with a larger budget.
//
// A bare timeout cuts pricing time by orders of magnitude, but when the solve that would end
// column generation times out empty-handed, column generation stops unproven, at an LP value above
// the optimum (RC201_50 at 1 860.45 against 535.29). Escalating keeps the cheap truncated solves
// and makes the last one complete.
//
// Deliberately free of Gurobi and of `VRP`, so the test suite can check the rule without a solver.

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"

/// @brief Whether a pricing solve that found no improving column proves there is none.
///
/// Only a `COMPLETE` status says the search was exhaustive, and even then a memory-pressure trim or
/// a join stopped at `max_join_pairs` leaves labels or pairs unexamined: see
/// `BidirectionalStats::join_truncated`.
///
/// @param result The solve's result.
/// @return Whether "no improving column" from this solve is a proof.
[[nodiscard]] inline bool proves_no_column(const rcspp::SolveResult& result) {
    return result.status == rcspp::AlgorithmStatus::COMPLETE && !result.memory_pressure_triggered &&
           !(result.bidirectional.has_value() && result.bidirectional->join_truncated);
}

/// @brief The params for an algorithm that wraps @p inner: the defaults, with @p inner's timeout.
///
/// A recorder that hands every solve to @p inner starts from these and passes its own timeout on
/// before each solve, so a budget set on the recorder reaches the search.
///
/// @param inner The wrapped algorithm.
/// @return Default params whose `timeout_s` is @p inner's current timeout.
template <typename ResourceType, typename LabelContainerType>
[[nodiscard]] rcspp::AlgorithmParams<LabelContainerType> params_timed_as(
    const rcspp::Algorithm<ResourceType, LabelContainerType>& inner) {
    rcspp::AlgorithmParams<LabelContainerType> params;
    params.timeout_s = inner.timeout();
    return params;
}

/// @brief How long each pricing solve may run, and what to do when that was too short.
///
/// A solve that times out with no improving column has its budget multiplied by @ref growth, and
/// the same duals are priced again, up to @ref max_seconds. With the default infinite
/// @ref max_seconds the last attempt is always long enough to finish, so column generation ends
/// proven; the attempts before it cost at most `1 / (growth - 1)` of it, 11 % at the default.
struct PricingBudget {
        /// @brief Seconds per pricing solve at first. Infinity, the default, means no budget:
        ///        each algorithm keeps its own `timeout_s`, exactly as before.
        double seconds = std::numeric_limits<double>::infinity();
        /// @brief The factor an escalation multiplies the budget by. Must exceed 1.
        double growth = 10.0;  // NOLINT(readability-magic-numbers)
        /// @brief The largest budget; an attempt at it is the last one.
        double max_seconds = std::numeric_limits<double>::infinity();
        /// @brief Whether an escalated budget carries over to the next iteration (true) or each
        ///        iteration starts again from @ref seconds (false). A change of pricing model -- a
        ///        convergence hook returning true -- always resets it.
        bool sticky = true;

        /// @brief Whether a budget is set at all.
        [[nodiscard]] bool enabled() const { return std::isfinite(seconds); }

        /// @brief Whether a budget of @p current may still grow.
        [[nodiscard]] bool can_grow(double current) const {
            return enabled() && current < max_seconds;
        }

        /// @brief The budget after one escalation from @p current, capped at @ref max_seconds.
        [[nodiscard]] double grown(double current) const {
            const double next = current * growth;
            return next < max_seconds ? next : max_seconds;
        }

        /// @brief Checks the fields.
        /// @throws std::invalid_argument naming the first field out of range.
        void check() const {
            if (!(seconds > 0.0)) {
                throw std::invalid_argument("PricingBudget: seconds must be positive");
            }
            if (!(growth > 1.0) || !std::isfinite(growth)) {
                throw std::invalid_argument("PricingBudget: growth must be finite and exceed 1");
            }
            if (!(max_seconds >= seconds)) {
                throw std::invalid_argument("PricingBudget: max_seconds must be >= seconds");
            }
        }
};

/// @brief What one pricing attempt found, reduced to what escalation decides on.
struct PricingAttempt {
        bool improving = false;  ///< it returned a column of negative reduced cost
        bool proof = false;      ///< it proves there is none (see `proves_no_column`)
        bool timed_out = false;  ///< its status was `TIMEOUT`, the one thing more time can fix
};

/// @brief Prices with @p attempt, growing `*current` while time is what is missing.
///
/// Escalates only on a timeout: a solve that failed to prove for any other reason -- a truncated
/// join, a memory trim, a solution or phase cap -- would fail the same way with more time, and
/// with an infinite cap would be retried for ever.
///
/// @param budget  The budget's rule.
/// @param current In: the budget to try first. Out: the budget of the last attempt.
/// @param attempt Called with a budget in seconds; returns what it found.
/// @return The last attempt's outcome, and how many escalations it took.
template <typename Attempt>
std::pair<PricingAttempt, size_t> price_with_escalation(const PricingBudget& budget,
                                                        double* current, Attempt&& attempt) {
    size_t escalations = 0;
    for (;;) {
        const PricingAttempt outcome = attempt(*current);
        if (outcome.improving || outcome.proof || !outcome.timed_out ||
            !budget.can_grow(*current)) {
            return {outcome, escalations};
        }
        *current = budget.grown(*current);
        ++escalations;
    }
}
