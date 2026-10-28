// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/master_problem.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/utils/logger.hpp"

namespace rcspp::column_generation {

RowRange MasterProblem::add_rows(size_t count, RowSense sense, double right_hand_side) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    const double lower = sense == RowSense::LessEqual ? -infinity : right_hand_side;
    const double upper = sense == RowSense::GreaterEqual ? infinity : right_hand_side;

    const std::vector<double> lowers(count, lower);
    const std::vector<double> uppers(count, upper);
    return add_rows(lowers, uppers);
}

RowRange MasterProblem::add_rows(std::span<const double> lower, std::span<const double> upper,
                                 std::span<const std::string> names) {
    if (!solver_.capabilities().add_rows) {
        throw std::logic_error(
            "MasterProblem::add_rows: this master solver cannot add rows; build them in the "
            "wrapped model instead");
    }
    if (lower.size() != upper.size() || (!names.empty() && names.size() != lower.size())) {
        throw std::invalid_argument(
            "MasterProblem::add_rows: lower, upper and names must have the same size");
    }

    const RowRange range{.first = solver_.number_of_rows(), .count = lower.size()};
    solver_.add_rows(RowBatch{.lower = lower, .upper = upper, .names = names});
    return range;
}

size_t MasterProblem::add_pricer(Pricer& pricer, ConvexityRow convexity) {
    std::optional<size_t> row = convexity.existing_row;

    if (std::isfinite(convexity.max_paths)) {
        if (row.has_value()) {
            throw std::invalid_argument(
                "MasterProblem::add_pricer: set either existing_row or max_paths, not both");
        }
        row = add_rows(1, RowSense::LessEqual, convexity.max_paths).first;
    } else if (row.has_value() && *row >= solver_.number_of_rows()) {
        throw std::out_of_range("MasterProblem::add_pricer: existing_row is not a row");
    }

    pricers_.push_back({.pricer = &pricer, .convexity_row = row});
    return pricers_.size() - 1;
}

void MasterProblem::add_columns(size_t pricer, std::vector<Solution> columns) {
    if (pricer >= pricers_.size()) {
        throw std::out_of_range("MasterProblem::add_columns: no such pricer");
    }
    if (columns.empty()) {
        return;
    }

    // Build the batch: each column's rows, checked, then its convexity entry.
    const std::optional<size_t> convexity = pricers_[pricer].convexity_row;
    const size_t row_count = solver_.number_of_rows();
    clear_batch();
    for (const Solution& solution : columns) {
        begin_batch_column(solution.column.cost);
        add_column_rows(solution.column.rows, row_count, convexity);
        if (convexity.has_value()) {
            add_batch_entry(*convexity, 1.0);
        }
    }

    // Send it, then record the columns, keeping their solutions.
    send_batch();

    for (Solution& solution : columns) {
        columns_.push_back({.id = columns_.size(),
                            .pricer = pricer,
                            .tag = ColumnTag::Real,
                            .solution = std::move(solution)});
    }
}

void MasterProblem::add_artificial_columns(std::span<const size_t> rows,
                                           std::span<const double> coefficients, double cost) {
    if (rows.size() != coefficients.size()) {
        throw std::invalid_argument(
            "MasterProblem::add_artificial_columns: rows and coefficients must have the same "
            "size");
    }

    const size_t row_count = solver_.number_of_rows();
    clear_batch();
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i] >= row_count) {
            throw std::out_of_range(
                "MasterProblem::add_artificial_columns: not a row of the master");
        }
        begin_batch_column(cost);
        add_batch_entry(rows[i], coefficients[i]);
    }

    send_batch();

    for (size_t i = 0; i < rows.size(); ++i) {
        artificial_column_ids_.push_back(columns_.size());
        columns_.push_back({.id = columns_.size(),
                            .pricer = no_pricer,
                            .tag = ColumnTag::Artificial,
                            .solution = {}});
    }
}

void MasterProblem::add_column_rows(std::span<const Row> rows, size_t row_count,
                                    std::optional<size_t> convexity) {
    column_rows_.clear();
    for (const Row& row : rows) {
        if (row.index >= row_count) {
            throw std::out_of_range(
                "MasterProblem::add_columns: a column has a row index that is not a row of the "
                "master");
        }
        if (convexity.has_value() && row.index == *convexity) {
            throw std::invalid_argument(
                "MasterProblem::add_columns: a column already has an entry in its pricer's "
                "convexity row");
        }
        column_rows_.emplace_back(row.index, static_cast<double>(row.coefficient));
    }

    // A graph pricer gives each row once, in order; other columns are sorted, and a row listed
    // twice gets the sum, as an LP solver would read it.
    const auto out_of_order = [](const auto& first, const auto& second) {
        return first.first >= second.first;
    };
    if (std::ranges::adjacent_find(column_rows_, out_of_order) != column_rows_.end()) {
        std::ranges::sort(column_rows_);
        size_t kept = 0;
        bool repeated = false;
        for (const auto& [row, value] : column_rows_) {
            if (kept > 0 && column_rows_[kept - 1].first == row) {
                column_rows_[kept - 1].second += value;
                repeated = true;
            } else {
                column_rows_[kept++] = {row, value};
            }
        }
        column_rows_.resize(kept);

        if (repeated && !warned_repeated_rows_) {
            warned_repeated_rows_ = true;
            LOG_WARN(
                "MasterProblem::add_columns: a column lists a row more than once; its "
                "entries are added together\n");
        }
    }

    for (const auto& [row, value] : column_rows_) {
        add_batch_entry(row, value);
    }
}

void MasterProblem::clear_batch() {
    batch_ids_.clear();
    batch_costs_.clear();
    batch_starts_.clear();
    batch_row_indices_.clear();
    batch_values_.clear();
}

void MasterProblem::begin_batch_column(double cost) {
    batch_ids_.push_back(columns_.size() + batch_ids_.size());
    batch_costs_.push_back(cost);
    batch_starts_.push_back(static_cast<std::uint32_t>(batch_row_indices_.size()));
}

void MasterProblem::add_batch_entry(size_t row, double value) {
    batch_row_indices_.push_back(static_cast<std::uint32_t>(row));
    batch_values_.push_back(value);
}

// Closes the batch (end marker, bounds) and sends it in one master solver call.
void MasterProblem::send_batch() {
    const size_t count = batch_ids_.size();
    batch_starts_.push_back(static_cast<std::uint32_t>(batch_row_indices_.size()));
    batch_lowers_.assign(count, 0.0);
    batch_uppers_.assign(count, std::numeric_limits<double>::infinity());

    solver_.add_columns(ColumnBatch{.ids = batch_ids_,
                                    .cost = batch_costs_,
                                    .lower = batch_lowers_,
                                    .upper = batch_uppers_,
                                    .starts = batch_starts_,
                                    .row_indices = batch_row_indices_,
                                    .values = batch_values_});
}

}  // namespace rcspp::column_generation
