// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/algorithm/solution.hpp"
#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief Where a master column comes from.
enum class ColumnTag : std::uint8_t {
    Real,        ///< a column found by a pricer, or a starting column given for one
    Artificial,  ///< a placeholder that makes the first LP feasible (@c MasterInitialization)
};

/// @brief A column of the master and what the master knows about it.
struct MasterColumn {
        /// @brief Its id in the master solver.
        ColumnId id = 0;
        /// @brief Index of the pricer it belongs to; @c MasterProblem::no_pricer for an
        ///        artificial column.
        size_t pricer = 0;
        /// @brief Real or artificial.
        ColumnTag tag = ColumnTag::Real;
        /// @brief The pricer's solution: for a graph pricer, the path and its column; empty
        ///        for an artificial column.
        Solution solution;
};

/// @brief A block of consecutive rows: `first … first + count − 1`.
struct RowRange {
        /// @brief Number of the first row.
        size_t first = 0;
        /// @brief Number of rows.
        size_t count = 0;
};

/// @brief The convexity row of a pricer: "at most `max_paths` columns of this pricer".
///
/// Set at most one of the two fields. Neither set: the pricer has no convexity row.
struct ConvexityRow {
        /// @brief A row the user already built, e.g. in a wrapped model.
        std::optional<size_t> existing_row;
        /// @brief The master creates the row `Σ x_p ≤ max_paths` (needs @c add_rows).
        double max_paths = std::numeric_limits<double>::infinity();
};

/// @brief The restricted master problem: rows, pricers and the columns generated so far.
///
/// Holds the master solver by reference: the caller keeps it, and every pricer, alive.
/// Columns get consecutive ids from 0; columns are never removed for now, so the id of a
/// column is also its position in @c columns().
class MasterProblem {
    public:
        /// @brief Value of @c MasterColumn::pricer for an artificial column.
        static constexpr size_t no_pricer = std::numeric_limits<size_t>::max();

        /// @brief Constructs a master over @p solver.
        ///
        /// @param solver The LP solver holding the master. Its existing rows are the master's
        ///                rows. It may hold variables of its own (a user's slacks or starting
        ///                columns): the master ignores them.
        explicit MasterProblem(MasterSolver& solver) : solver_(solver) {}

        /// @brief Appends @p count rows with the same sense and right-hand side.
        ///
        /// @param count           Number of rows.
        /// @param sense           `=`, `≥` or `≤`.
        /// @param right_hand_side The right-hand side of every row.
        /// @return The rows created.
        /// @throws std::logic_error When the master solver cannot add rows.
        RowRange add_rows(size_t count, RowSense sense, double right_hand_side);

        /// @brief Appends rows `lower ≤ a·x ≤ upper`.
        ///
        /// @param lower Lower bound of each row.
        /// @param upper Upper bound of each row.
        /// @param names Optional names, one per row, for error messages and debugging.
        /// @return The rows created.
        /// @throws std::logic_error      When the master solver cannot add rows.
        /// @throws std::invalid_argument When the sizes differ.
        RowRange add_rows(std::span<const double> lower, std::span<const double> upper,
                          std::span<const std::string> names = {});

        /// @brief Registers a pricer and its convexity row.
        ///
        /// @param pricer    The pricer; kept by reference.
        /// @param convexity Its convexity row, if any.
        /// @return The pricer's index `k`.
        /// @throws std::invalid_argument When both fields of @p convexity are set.
        /// @throws std::out_of_range     When @c ConvexityRow::existing_row is not a row.
        size_t add_pricer(Pricer& pricer, ConvexityRow convexity = {});

        /// @brief Adds columns of pricer @p pricer: starting columns or generated ones.
        ///
        /// Each column takes the cost and rows of @c Solution::column, plus a 1 in the pricer's
        /// convexity row. A row listed more than once in a column gets the sum of its entries (with
        /// one warning per master: it is usually a mistake). The solutions are moved into the
        /// master.
        ///
        /// @param pricer  Index of the pricer the columns belong to.
        /// @param columns The pricer's solutions.
        /// @throws std::out_of_range     When @p pricer or a row index does not exist.
        /// @throws std::invalid_argument When a column already has an entry in the convexity
        ///                               row.
        void add_columns(size_t pricer, std::vector<Solution> columns);

        /// @brief Adds artificial columns: one per row, each with a single entry.
        ///
        /// @param rows         The row of each column.
        /// @param coefficients Its coefficient in that row.
        /// @param cost         The cost of every column.
        /// @throws std::invalid_argument When the sizes differ.
        /// @throws std::out_of_range     When a row does not exist.
        void add_artificial_columns(std::span<const size_t> rows,
                                    std::span<const double> coefficients, double cost);

        /// @brief Number of rows.
        [[nodiscard]] size_t number_of_rows() const { return solver_.number_of_rows(); }

        /// @brief Number of columns, artificial ones included.
        [[nodiscard]] size_t number_of_columns() const { return columns_.size(); }

        /// @brief Number of pricers.
        [[nodiscard]] size_t number_of_pricers() const { return pricers_.size(); }

        /// @brief Pricer @p index.
        [[nodiscard]] Pricer& pricer(size_t index) const { return *pricers_.at(index).pricer; }

        /// @brief Convexity row of pricer @p index, if it has one.
        [[nodiscard]] std::optional<size_t> convexity_row(size_t index) const {
            return pricers_.at(index).convexity_row;
        }

        /// @brief Every column, in id order.
        [[nodiscard]] std::span<const MasterColumn> columns() const { return columns_; }

        /// @brief Ids of the artificial columns.
        [[nodiscard]] std::span<const ColumnId> artificial_column_ids() const {
            return artificial_column_ids_;
        }

        /// @brief Whether a loop has initialized this master (e.g. added artificial columns).
        [[nodiscard]] bool initialized() const { return initialized_; }

        /// @brief Records that the master is initialized, so no other loop does it again.
        void mark_initialized() { initialized_ = true; }

        /// @brief The master solver.
        [[nodiscard]] MasterSolver& solver() const { return solver_; }

    private:
        struct PricerEntry {
                Pricer* pricer;
                std::optional<size_t> convexity_row;
        };

        void clear_batch();
        void begin_batch_column(double cost);
        void add_batch_entry(size_t row, double value);
        // Adds a column's rows to the batch, checked; a row listed twice gets the sum.
        void add_column_rows(std::span<const Row> rows, size_t row_count,
                             std::optional<size_t> convexity);
        // Closes the batch and sends it in one master solver call.
        void send_batch();

        MasterSolver& solver_;
        std::vector<PricerEntry> pricers_;
        std::vector<MasterColumn> columns_;
        std::vector<ColumnId> artificial_column_ids_;
        bool warned_repeated_rows_ = false;
        bool initialized_ = false;

        // Batch buffers, reused between calls so adding columns does not allocate once warm.
        std::vector<ColumnId> batch_ids_;
        std::vector<double> batch_costs_;
        std::vector<double> batch_lowers_;
        std::vector<double> batch_uppers_;
        std::vector<std::uint32_t> batch_starts_;
        std::vector<std::uint32_t> batch_row_indices_;
        std::vector<double> batch_values_;
        std::vector<std::pair<size_t, double>> column_rows_;
};

}  // namespace rcspp::column_generation
