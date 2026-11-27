// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/debug_checks.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rcspp::column_generation {

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();

// Throws for the first failure, telling how many more there are.
void throw_if_any(const char* check, size_t iteration, const std::vector<std::string>& failures,
                  double tolerance) {
    if (failures.empty()) {
        return;
    }

    const std::string more =
        failures.size() > 1 ? std::format(" ({} more)", failures.size() - 1) : std::string();
    throw DebugCheckError(check,
                          iteration,
                          std::format("{}{}; tolerance {:g}", failures.front(), more, tolerance));
}

// A row's bounds, for a message.
std::string bounds(double lower, double upper) {
    if (lower == -infinity && upper == infinity) {
        return "no bound";
    }

    if (lower == -infinity) {
        return std::format("<= {}", upper);
    }

    if (upper == infinity) {
        return std::format(">= {}", lower);
    }

    return std::format("{} ... {}", lower, upper);
}

}  // namespace

DebugCheckError::DebugCheckError(std::string check, size_t iteration, std::string message)
    : std::runtime_error(
          std::format("debug check {} failed at iteration {}: {}", check, iteration, message)),
      check_(std::move(check)),
      iteration_(iteration),
      message_(std::move(message)) {}

// MARK: The master's columns

void keep_new_columns(KeptColumns& kept, const MasterProblem& master) {
    const std::span<const MasterColumn> columns = master.columns();

    for (size_t id = kept.costs.size(); id < columns.size(); ++id) {
        const MasterColumn& column = columns[id];
        kept.costs.push_back(column.solution.column.cost);

        for (const Row& row : column.solution.column.rows) {
            kept.rows.push_back(row.index);
            kept.values.push_back(static_cast<double>(row.coefficient));
            kept.owners.push_back(id);
        }

        const std::optional<size_t> convexity = column.pricer == MasterProblem::no_pricer
                                                    ? std::nullopt
                                                    : master.convexity_row(column.pricer);

        if (convexity.has_value()) {
            kept.rows.push_back(*convexity);
            kept.values.push_back(1.0);
            kept.owners.push_back(id);
        }
    }
}

// MARK: The checks

void check_pricer_reduced_costs(std::span<const Solution> columns, std::span<const double> duals,
                                double tolerance, size_t iteration, size_t pricer) {
    std::vector<std::string> failures;

    for (size_t position = 0; position < columns.size(); ++position) {
        const Solution& solution = columns[position];
        const bool known_rows = std::ranges::all_of(solution.column.rows, [&duals](const Row& row) {
            return row.index < duals.size();
        });

        if (!known_rows) {
            continue;
        }

        double recomputed = solution.column.cost;
        double scale = std::abs(solution.column.cost);

        for (const Row& row : solution.column.rows) {
            const double term = static_cast<double>(row.coefficient) * duals[row.index];
            recomputed -= term;
            scale += std::abs(term);
        }

        if (!(std::abs(solution.cost - recomputed) <= tolerance * std::max(1.0, scale))) {
            failures.push_back(std::format(
                "pricer {}'s column {} reports the reduced cost {}, its cost and rows give {}",
                pricer,
                position,
                solution.cost,
                recomputed));
        }
    }

    throw_if_any("pricer_reduced_costs", iteration, failures, tolerance);
}

void check_dual_signs(std::span<const double> duals, std::span<const double> lower,
                      std::span<const double> upper, double tolerance, size_t iteration) {
    std::vector<std::string> failures;

    for (size_t row = 0; row < duals.size(); ++row) {
        const bool no_lower = lower[row] == -infinity;
        const bool no_upper = upper[row] == infinity;
        const double dual = duals[row];
        const bool wrong = (no_lower && !no_upper && dual > tolerance) ||
                           (!no_lower && no_upper && dual < -tolerance) ||
                           (no_lower && no_upper && std::abs(dual) > tolerance);

        if (wrong) {
            failures.push_back(std::format("row {} ({}) has the dual {}",
                                           row,
                                           bounds(lower[row], upper[row]),
                                           dual));
        }
    }

    throw_if_any("dual_signs", iteration, failures, tolerance);
}

void check_master_reduced_costs(const KeptColumns& kept, std::span<const double> duals,
                                std::span<const double> values, double tolerance,
                                double feasibility, size_t iteration) {
    // Each column's reduced cost, and the size of what made it.
    std::vector<double> reduced_costs = kept.costs;
    std::vector<double> scales(kept.costs.size());

    for (size_t column = 0; column < kept.costs.size(); ++column) {
        scales[column] = std::abs(kept.costs[column]);
    }

    for (size_t entry = 0; entry < kept.rows.size(); ++entry) {
        const double term = kept.values[entry] * duals[kept.rows[entry]];
        reduced_costs[kept.owners[entry]] -= term;
        scales[kept.owners[entry]] += std::abs(term);
    }

    // The negative ones first, then the used ones that are not 0.
    std::vector<std::string> negative;
    std::vector<std::string> used;

    for (size_t column = 0; column < kept.costs.size(); ++column) {
        const double margin = tolerance * std::max(1.0, scales[column]);
        const double reduced_cost = reduced_costs[column];

        if (reduced_cost < -margin) {
            negative.push_back(
                std::format("column {} has the reduced cost {}", column, reduced_cost));
        } else if (values[column] > feasibility && std::abs(reduced_cost) > margin) {
            used.push_back(std::format("column {}, of value {}, has the reduced cost {}",
                                       column,
                                       values[column],
                                       reduced_cost));
        }
    }

    negative.insert(negative.end(), used.begin(), used.end());
    throw_if_any("master_reduced_costs", iteration, negative, tolerance);
}

void check_objective(const KeptColumns& kept, std::span<const double> values, double objective,
                     double tolerance, size_t iteration) {
    double recomputed = 0.0;
    double scale = 0.0;

    for (size_t column = 0; column < kept.costs.size(); ++column) {
        const double cost = kept.costs[column] * values[column];
        recomputed += cost;
        scale += std::abs(cost);
    }

    std::vector<std::string> failures;

    if (!(std::abs(objective - recomputed) <= tolerance * std::max(1.0, scale))) {
        failures.push_back(
            std::format("the LP value is {}, its columns' values cost {}", objective, recomputed));
    }

    throw_if_any("objective", iteration, failures, tolerance);
}

}  // namespace rcspp::column_generation
