// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Checks that a run's master solver and pricers agree with each other, for debugging a master
// solver or a pricer: each is off by default, and a failed one throws DebugCheckError.

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "rcspp/algorithm/solution.hpp"
#include "rcspp/column_generation/master_problem.hpp"

namespace rcspp::column_generation {

/// @brief Which checks a run makes. Each is off by default; a failed check throws
///        @c DebugCheckError.
///
/// They all use the run's reduced-cost tolerance (`tol` below): the larger of
/// @c Tolerances::reduced_cost and @c MasterSolver::dual_tolerance.
struct DebugChecks {
        /// @brief Each column a pricer returns: its reduced cost (@c Solution::cost) is its
        ///        column's cost minus the duals of its rows, the convexity row's left out, within
        ///        `tol` times `max(1, |cost| + sum |dual * coefficient|)`. Costs the new columns'
        ///        entries.
        bool pricer_reduced_costs = false;
        /// @brief The duals of each LP solve against their rows' bounds: at most `tol` on a `<=`
        ///        row, at least `-tol` on a `>=` row, within `tol` of 0 on a row with no bound. A
        ///        row with two different finite bounds is not checked: its sign depends on the
        ///        bound it reaches. Costs a pass over the rows.
        bool dual_signs = false;
        /// @brief The reduced cost of every column the master holds, from its cost, its entries
        ///        and the duals, after each LP solve: at least `-tol` (scaled as for
        ///        @c pricer_reduced_costs), and within `tol` of 0 for a column the LP solution
        ///        uses. Variables of the user's own model are not seen. Costs every entry of the
        ///        master and a read of every column's value.
        bool master_reduced_costs = false;
        /// @brief The LP value equals the cost of its columns' values, within `tol` times
        ///        `max(1, sum |cost * value|)`. Checked only when the user's own variables cost
        ///        nothing (@c MasterSolver::largest_user_cost is 0); an objective constant in the
        ///        user's model fails it. Costs a read of every column's value.
        bool objective = false;
};

/// @brief A debug check failed (@c DebugChecks): the master solver or a pricer gave something
///        the others contradict.
class DebugCheckError : public std::runtime_error {
    public:
        /// @param check     The check's name, a field of @c DebugChecks.
        /// @param iteration The iteration it failed at.
        /// @param message   What failed.
        DebugCheckError(std::string check, size_t iteration, std::string message);

        /// @brief The check's name, a field of @c DebugChecks.
        [[nodiscard]] const std::string& check() const { return check_; }
        /// @brief The iteration it failed at.
        [[nodiscard]] size_t iteration() const { return iteration_; }
        /// @brief What failed, without the check and the iteration.
        [[nodiscard]] const std::string& message() const { return message_; }

    private:
        std::string check_;
        size_t iteration_;
        std::string message_;
};

/// @brief The master's columns as arrays, for the checks over the whole master: one cost per
///        column, and per entry its row, its value and its column (the column's id).
struct KeptColumns {
        std::vector<double> costs;
        std::vector<size_t> rows;
        std::vector<double> values;
        std::vector<size_t> owners;
};

/// @brief Appends the columns @p master gained since the last call: each column's own entries,
///        then a 1 in its pricer's convexity row (as @c MasterProblem::add_columns sends it).
void keep_new_columns(KeptColumns& kept, const MasterProblem& master);

/// @brief Checks the reduced cost each of pricer @p pricer's columns reports. A column whose
///        row does not exist is left to @c MasterProblem::add_columns, which refuses it.
/// @throws DebugCheckError When one is off.
void check_pricer_reduced_costs(std::span<const Solution> columns, std::span<const double> duals,
                                double tolerance, size_t iteration, size_t pricer);

/// @brief Checks the sign of each row's dual against the row's bounds (minimization,
///        `dual = d objective / d bound`).
/// @throws DebugCheckError When one is wrong.
void check_dual_signs(std::span<const double> duals, std::span<const double> lower,
                      std::span<const double> upper, double tolerance, size_t iteration);

/// @brief Checks that no column of the master has a negative reduced cost, and that the columns
///        the LP solution uses (@p values, by column id) have none but 0.
/// @throws DebugCheckError When one does.
void check_master_reduced_costs(const KeptColumns& kept, std::span<const double> duals,
                                std::span<const double> values, double tolerance,
                                double feasibility, size_t iteration);

/// @brief Checks the LP value against the cost of its columns' values (@p values, by id).
/// @throws DebugCheckError When they differ.
void check_objective(const KeptColumns& kept, std::span<const double> values, double objective,
                     double tolerance, size_t iteration);

}  // namespace rcspp::column_generation
