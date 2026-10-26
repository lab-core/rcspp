// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <string>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"

namespace rcspp::column_generation {

/// @brief Default absolute tolerance of @c master_solver_self_test on the values it compares.
inline constexpr double default_self_test_tolerance = 1e-6;

/// @brief Outcome of @c master_solver_self_test.
struct MasterSolverSelfTestReport {
        /// @brief One line per failed check: the model, the step, what was checked, the
        ///        expected value and the value the master solver gave.
        std::vector<std::string> failures;

        /// @brief Whether every check passed.
        [[nodiscard]] bool passed() const { return failures.empty(); }
};

/// @brief Checks that a master solver follows the conventions of @c MasterSolver.
///
/// The column generation loop trusts the answers of its master solver: a dual with the wrong
/// sign gives wrong reduced costs without any error. This test solves an LP whose answers are
/// known, through the interface only, so it works for any master solver:
///
///     min  x1 + 3 x2 + x3
///     s.t. x1 + x2      = 2      (row 0)
///          x2 + x3      ≥ 1      (row 1)
///          x1 + x3      ≤ 1.5    (row 2)
///          x ≥ 0
///
/// Its optimum is 3.75 at `x = (1.25, 0.75, 0.25)`, with the duals `(1.5, 1.5, −0.5)`: one row
/// of each kind, each dual of its expected sign. The test then adds a column `x4` of cost 0.5
/// in rows 0 and 1 and solves again: 1.0 at `x4 = 2`, with the duals `(0.5, 0, 0)`. Both optima
/// are unique, primal and dual. The steps, on each of the two master solvers:
///
/// 1. add the three rows, and read their bounds back;
/// 2. add `x1`, `x2`, `x3` under ids that are neither their positions nor sorted, solve, and
///    check the objective, the duals and the values read by id in another order;
/// 3. add `x4`, solve again, and check the new optimum;
/// 4. when @c Capabilities::integer_solve is set, solve the integer problem: 1.0 at `x4 = 2`.
///
/// A step whose solve does not end @c SolverStatus::Optimal is reported and ends the checks on
/// that master solver.
///
/// @param minimizing A master solver over an empty minimizing model, able to add rows.
/// @param maximizing A master solver of the same kind over an empty maximizing model: it must
///                   give the same answers, as the master is always seen as a minimization.
/// @param tolerance  Absolute tolerance on the values compared.
/// @return The failed checks of both master solvers.
/// @throws std::invalid_argument When a master solver already has rows or cannot add rows.
[[nodiscard]] MasterSolverSelfTestReport master_solver_self_test(
    MasterSolver& minimizing, MasterSolver& maximizing,
    double tolerance = default_self_test_tolerance);

}  // namespace rcspp::column_generation
