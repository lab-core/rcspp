// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// What every master solver needs whatever its solver: which solver column holds each column id,
// and which bounds a row can hold. Written once, so that master solvers cannot drift apart.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief Whether a row `lower ≤ a·x ≤ upper` can have these bounds: numbers, with
///        `lower < +∞`, `upper > −∞` and `lower ≤ upper`.
[[nodiscard]] inline bool row_bounds_can_be_held(double lower, double upper) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    return !std::isnan(lower) && !std::isnan(upper) && lower != infinity && upper != -infinity &&
           lower <= upper;
}

/// @brief The solver column of every column a master solver received, by id.
///
/// @tparam SolverColumn How the solver names a column: an object (Gurobi's @c GRBVar) or an
///                      index (a HiGHS column).
template <typename SolverColumn>
class SolverColumns {
    public:
        /// @brief Checks a batch before anything of it is added, so that a refused batch adds
        ///        nothing.
        ///
        /// @param batch The columns to add.
        /// @param rows  The number of rows of the master solver.
        /// @throws std::invalid_argument When an id is already used, or used twice in the batch.
        /// @throws std::out_of_range When a column enters a row that does not exist.
        void check(const ColumnBatch& batch, size_t rows) const {
            std::unordered_set<ColumnId> batch_ids;
            for (const ColumnId id : batch.ids) {
                if (positions_.contains(id) || !batch_ids.insert(id).second) {
                    throw std::invalid_argument("add_columns: a column id is already used");
                }
            }
            for (const std::uint32_t row : batch.row_indices) {
                if (row >= rows) {
                    throw std::out_of_range(
                        "add_columns: a column enters a row that does not exist");
                }
            }
        }

        /// @brief Records that the solver holds column @p id as @p column.
        void add(ColumnId id, SolverColumn column) {
            positions_.emplace(id, columns_.size());
            columns_.push_back(std::move(column));
        }

        /// @brief The solver column of column @p id.
        /// @throws std::out_of_range For an id never added.
        [[nodiscard]] const SolverColumn& at(ColumnId id) const {
            const auto found = positions_.find(id);
            if (found == positions_.end()) {
                throw std::out_of_range("no column has this id");
            }
            return columns_[found->second];
        }

        /// @brief Every solver column, in the order they were added.
        [[nodiscard]] std::span<const SolverColumn> all() const { return columns_; }

    private:
        std::vector<SolverColumn> columns_;
        std::unordered_map<ColumnId, size_t> positions_;
};

}  // namespace rcspp::column_generation
