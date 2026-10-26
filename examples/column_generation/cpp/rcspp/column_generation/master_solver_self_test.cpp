// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/master_solver_self_test.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();

// The rows of the LP described in the header.
constexpr std::array<double, 3> row_lower{2.0, 1.0, -infinity};
constexpr std::array<double, 3> row_upper{2.0, infinity, 1.5};
constexpr std::array<std::string_view, 3> row_names{"row 0 (=)", "row 1 (>=)", "row 2 (<=)"};

// x1, x2, x3, under ids that are neither their positions nor sorted.
constexpr std::array<ColumnId, 3> first_ids{7, 3, 5};
constexpr std::array<double, 3> first_costs{1.0, 3.0, 1.0};
constexpr std::array<double, 3> first_lower{0.0, 0.0, 0.0};
constexpr std::array<double, 3> first_upper{infinity, infinity, infinity};
constexpr std::array<std::uint32_t, 4> first_starts{0, 2, 4, 6};
constexpr std::array<std::uint32_t, 6> first_rows{0, 2, 0, 1, 1, 2};
constexpr std::array<double, 6> first_values{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

// x4: cost 0.5 in rows 0 and 1.
constexpr std::array<ColumnId, 1> added_id{9};
constexpr std::array<double, 1> added_cost{0.5};
constexpr std::array<double, 1> added_lower{0.0};
constexpr std::array<double, 1> added_upper{infinity};
constexpr std::array<std::uint32_t, 2> added_starts{0, 2};
constexpr std::array<std::uint32_t, 2> added_rows{0, 1};
constexpr std::array<double, 2> added_values{1.0, 1.0};

// The optimum of the first LP, and of the LP with x4.
constexpr double first_objective = 3.75;
constexpr std::array<double, 3> first_duals{1.5, 1.5, -0.5};
constexpr double added_objective = 1.0;
constexpr std::array<double, 3> added_duals{0.5, 0.0, 0.0};
constexpr double added_x4 = 2.0;

// The values are read in an order that differs from the order the columns were added in.
constexpr std::array<ColumnId, 3> first_read_ids{5, 7, 3};
constexpr std::array<std::string_view, 3> first_read_names{"x3", "x1", "x2"};
constexpr std::array<double, 3> first_read_values{0.25, 1.25, 0.75};
constexpr std::array<ColumnId, 2> added_read_ids{9, 7};
constexpr std::array<std::string_view, 2> added_read_names{"x4", "x1"};
constexpr std::array<double, 2> added_read_values{added_x4, 0.0};

constexpr std::array<std::string_view, 5> status_names{
    "Optimal", "Infeasible", "Unbounded", "TimeLimit", "Other"};

// Records the failed checks of one master solver, each line prefixed by the model and the step.
class Checker {
    public:
        Checker(std::string_view model, double tolerance, MasterSolverSelfTestReport& report)
            : model_(model), tolerance_(tolerance), report_(report) {}

        void set_step(std::string_view step) { step_ = step; }

        // Equal values pass too, so that infinite bounds compare.
        void near(std::string_view what, double expected, double actual) {
            if (expected == actual || std::abs(expected - actual) <= tolerance_) {
                return;
            }
            fail(std::format("{}: expected {}, got {}", what, expected, actual));
        }

        // Whether the solve ended optimal; reports it when not.
        bool optimal(SolverStatus status) {
            if (status == SolverStatus::Optimal) {
                return true;
            }
            fail(std::format("status: expected Optimal, got {}",
                             status_names.at(static_cast<std::size_t>(status))));
            return false;
        }

    private:
        void fail(const std::string& line) {
            report_.failures.push_back(std::format("{}: {}: {}", model_, step_, line));
        }

        std::string_view model_;
        std::string_view step_;
        double tolerance_;
        MasterSolverSelfTestReport& report_;
};

void check_duals(Checker& check, const MasterSolver& solver, std::span<const double> expected) {
    std::vector<double> duals(expected.size());
    solver.duals(duals);
    for (std::size_t row = 0; row < expected.size(); ++row) {
        check.near(std::format("dual of {}", row_names.at(row)), expected[row], duals[row]);
    }
}

void check_values(Checker& check, const MasterSolver& solver, std::span<const ColumnId> ids,
                  std::span<const std::string_view> names, std::span<const double> expected) {
    std::vector<double> values(ids.size());
    solver.primal(ids, values);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        check.near(std::format("value of {} (id {})", names[index], ids[index]),
                   expected[index],
                   values[index]);
    }
}

void check_master_solver(MasterSolver& solver, std::string_view model, double tolerance,
                         MasterSolverSelfTestReport& report) {
    Checker check(model, tolerance, report);

    // 1. The rows, read back.
    check.set_step("rows");
    solver.add_rows(RowBatch{.lower = row_lower, .upper = row_upper});
    std::vector<double> lower(row_lower.size());
    std::vector<double> upper(row_upper.size());
    solver.row_bounds(lower, upper);
    for (std::size_t row = 0; row < row_lower.size(); ++row) {
        check.near(std::format("lower bound of {}", row_names.at(row)),
                   row_lower.at(row),
                   lower[row]);
        check.near(std::format("upper bound of {}", row_names.at(row)),
                   row_upper.at(row),
                   upper[row]);
    }

    // 2. x1, x2, x3.
    check.set_step("first LP");
    solver.add_columns(ColumnBatch{.ids = first_ids,
                                   .cost = first_costs,
                                   .lower = first_lower,
                                   .upper = first_upper,
                                   .starts = first_starts,
                                   .row_indices = first_rows,
                                   .values = first_values});
    if (!check.optimal(solver.solve_relaxation(RelaxationParams{}))) {
        return;
    }
    check.near("objective", first_objective, solver.objective());
    check_duals(check, solver, first_duals);
    check_values(check, solver, first_read_ids, first_read_names, first_read_values);

    // 3. x4, added after a solve.
    check.set_step("LP after adding x4");
    solver.add_columns(ColumnBatch{.ids = added_id,
                                   .cost = added_cost,
                                   .lower = added_lower,
                                   .upper = added_upper,
                                   .starts = added_starts,
                                   .row_indices = added_rows,
                                   .values = added_values});
    if (!check.optimal(solver.solve_relaxation(RelaxationParams{}))) {
        return;
    }
    check.near("objective", added_objective, solver.objective());
    check_duals(check, solver, added_duals);
    check_values(check, solver, added_read_ids, added_read_names, added_read_values);

    // 4. The integer problem, when the master solver solves it.
    if (!solver.capabilities().integer_solve) {
        return;
    }
    check.set_step("integer solve");
    if (!check.optimal(solver.solve_integer(IntegerParams{}))) {
        return;
    }
    check.near("objective", added_objective, solver.objective());
    check_values(check, solver, added_read_ids, added_read_names, added_read_values);
}

void require_empty_and_extensible(const MasterSolver& solver, std::string_view model) {
    if (solver.number_of_rows() != 0 || !solver.capabilities().add_rows) {
        throw std::invalid_argument(
            std::format("master_solver_self_test: the {} master solver must have no rows and "
                        "be able to add rows",
                        model));
    }
}

}  // namespace

MasterSolverSelfTestReport master_solver_self_test(MasterSolver& minimizing,
                                                   MasterSolver& maximizing, double tolerance) {
    require_empty_and_extensible(minimizing, "minimizing");
    require_empty_and_extensible(maximizing, "maximizing");

    MasterSolverSelfTestReport report;
    check_master_solver(minimizing, "minimizing model", tolerance, report);
    check_master_solver(maximizing, "maximizing model", tolerance, report);
    return report;
}

}  // namespace rcspp::column_generation
