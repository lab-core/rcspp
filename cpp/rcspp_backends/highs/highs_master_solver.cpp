// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp_backends/highs/highs_master_solver.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "rcspp/utils/logger.hpp"

namespace rcspp::column_generation {

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();

SolverStatus status_of(HighsModelStatus status) {
    switch (status) {
        case HighsModelStatus::kOptimal:
            return SolverStatus::Optimal;
        case HighsModelStatus::kInfeasible:
            return SolverStatus::Infeasible;
        case HighsModelStatus::kUnbounded:
            return SolverStatus::Unbounded;
        case HighsModelStatus::kTimeLimit:
            return SolverStatus::TimeLimit;
        default:
            return SolverStatus::Other;
    }
}

// Sets a string option for the lifetime of the guard, then puts the old value back.
class TemporaryOption {
    public:
        TemporaryOption(Highs& highs, std::string name, const std::string& value)
            : highs_(highs), name_(std::move(name)) {
            highs_.getOptionValue(name_, old_value_);
            highs_.setOptionValue(name_, value);
        }

        TemporaryOption(const TemporaryOption&) = delete;
        TemporaryOption& operator=(const TemporaryOption&) = delete;
        TemporaryOption(TemporaryOption&&) = delete;
        TemporaryOption& operator=(TemporaryOption&&) = delete;

        ~TemporaryOption() { highs_.setOptionValue(name_, old_value_); }

    private:
        Highs& highs_;
        std::string name_;
        std::string old_value_;
};

}  // namespace

HighsMasterSolver::HighsMasterSolver(Highs& highs, std::vector<HighsInt> rows)
    : highs_(highs), rows_(std::move(rows)) {
    const HighsInt model_rows = highs_.getNumRow();
    if (std::ranges::any_of(rows_,
                            [model_rows](HighsInt row) { return row < 0 || row >= model_rows; })) {
        throw std::out_of_range("HighsMasterSolver: a row index is not a row of the model");
    }
}

void HighsMasterSolver::row_bounds(std::span<double> lower, std::span<double> upper) const {
    // HiGHS takes bounds beyond infinite_bound as infinite.
    const HighsLp& model = highs_.getLp();
    const double infinite_bound = highs_.getOptions().infinite_bound;
    for (size_t row = 0; row < rows_.size(); ++row) {
        const double row_lower = model.row_lower_[rows_[row]];
        const double row_upper = model.row_upper_[rows_[row]];
        lower[row] = row_lower <= -infinite_bound ? -infinity : row_lower;
        upper[row] = row_upper >= infinite_bound ? infinity : row_upper;
    }
}

void HighsMasterSolver::add_rows(const RowBatch& rows) {
    // Check every row first, so that a refused batch adds nothing.
    for (size_t row = 0; row < rows.lower.size(); ++row) {
        const double lower = rows.lower[row];
        const double upper = rows.upper[row];
        if (!row_bounds_can_be_held(lower, upper)) {
            throw std::invalid_argument(
                "HighsMasterSolver::add_rows: row bounds must be numbers, with lower <= upper, "
                "lower < +inf and upper > -inf");
        }
    }

    const HighsInt first = highs_.getNumRow();
    const auto count = static_cast<HighsInt>(rows.lower.size());
    highs_.addRows(count, rows.lower.data(), rows.upper.data(), 0, nullptr, nullptr, nullptr);

    for (HighsInt row = 0; row < count; ++row) {
        const auto index = static_cast<size_t>(row);
        if (!rows.names.empty() && !rows.names[index].empty()) {
            highs_.passRowName(first + row, std::string(rows.names[index]));
        }
        rows_.push_back(first + row);
    }
}

void HighsMasterSolver::add_columns(const ColumnBatch& columns) {
    if (columns.size() == 0) {
        return;
    }

    columns_.check(columns, rows_.size());

    std::vector<HighsInt> model_rows;
    model_rows.reserve(columns.row_indices.size());
    for (const std::uint32_t row : columns.row_indices) {
        model_rows.push_back(rows_[row]);
    }

    const double cost_sign = sense_sign();
    std::vector<double> costs(columns.cost.begin(), columns.cost.end());
    for (double& cost : costs) {
        cost *= cost_sign;
    }
    const std::vector<HighsInt> starts(columns.starts.begin(), columns.starts.end() - 1);

    const HighsInt first = highs_.getNumCol();
    highs_.addCols(static_cast<HighsInt>(columns.size()),
                   costs.data(),
                   columns.lower.data(),
                   columns.upper.data(),
                   static_cast<HighsInt>(model_rows.size()),
                   starts.data(),
                   model_rows.data(),
                   columns.values.data());

    for (size_t column = 0; column < columns.size(); ++column) {
        columns_.add(columns.ids[column], first + static_cast<HighsInt>(column));
        upper_bounds_.push_back(columns.upper[column]);
    }
}

SolverStatus HighsMasterSolver::solve_relaxation(const RelaxationParams& params) {
    restore_upper_bounds();
    highs_.setOptionValue("solve_relaxation", true);
    set_solve_options(params.timeout_s, params.threads, params.verbose);
    return run(false);
}

double HighsMasterSolver::objective() const {
    return sense_sign() * highs_.getInfo().objective_function_value;
}

void HighsMasterSolver::duals(std::span<double> out) const {
    const double sign = sense_sign();
    const std::vector<double>& values = highs_.getSolution().row_dual;
    for (size_t row = 0; row < rows_.size(); ++row) {
        out[row] = sign * values[rows_[row]];
    }
}

void HighsMasterSolver::primal(std::span<const ColumnId> ids, std::span<double> out) const {
    const std::vector<double>& values = highs_.getSolution().col_value;
    for (size_t index = 0; index < ids.size(); ++index) {
        out[index] = values[column(ids[index])];
    }
}

SolverStatus HighsMasterSolver::solve_integer(const IntegerParams& params) {
    restore_upper_bounds();
    highs_.setOptionValue("solve_relaxation", false);
    set_solve_options(params.timeout_s, params.threads, params.verbose);
    highs_.setOptionValue("mip_rel_gap", params.relative_gap);

    const std::span<const HighsInt> generated = columns_.all();
    const auto count = static_cast<HighsInt>(generated.size());
    if (count > 0) {
        const std::vector<HighsVarType> types(generated.size(), HighsVarType::kInteger);
        highs_.changeColsIntegrality(count, generated.data(), types.data());

        // HiGHS has no binary type: an integer column with an upper bound of at most 1.
        if (params.column_type == ColumnType::Binary) {
            const HighsLp& model = highs_.getLp();
            std::vector<double> lower;
            std::vector<double> upper;
            for (size_t column = 0; column < generated.size(); ++column) {
                lower.push_back(model.col_lower_[generated[column]]);
                upper.push_back(std::min(upper_bounds_[column], 1.0));
            }
            highs_.changeColsBounds(count, generated.data(), lower.data(), upper.data());
            binary_bounds_ = true;
        }
    }
    return run(true);
}

void HighsMasterSolver::set_option(std::string_view name, const OptionValue& value) {
    const std::string option(name);

    const HighsStatus status = std::visit(
        [&](const auto& option_value) {
            using Value = std::decay_t<decltype(option_value)>;
            if constexpr (std::is_same_v<Value, std::int64_t>) {
                // An integer for a double option is taken as a double.
                HighsOptionType type = HighsOptionType::kInt;
                highs_.getOptionType(option, &type);
                if (type == HighsOptionType::kDouble) {
                    return highs_.setOptionValue(option, static_cast<double>(option_value));
                }
                return highs_.setOptionValue(option, static_cast<HighsInt>(option_value));
            } else {
                return highs_.setOptionValue(option, option_value);
            }
        },
        value);

    if (status == HighsStatus::kError) {
        throw std::invalid_argument("HighsMasterSolver::set_option: HiGHS refused the option '" +
                                    option + "' or its value");
    }
}

HighsInt HighsMasterSolver::column(ColumnId id) const {
    return columns_.at(id);
}

double HighsMasterSolver::sense_sign() const {
    ObjSense sense = ObjSense::kMinimize;
    highs_.getObjectiveSense(sense);
    return sense == ObjSense::kMaximize ? -1.0 : 1.0;
}

void HighsMasterSolver::set_solve_options(double timeout_s, int threads, bool verbose) {
    // HiGHS compares its time limit with the run time of all the model's solves so far.
    highs_.setOptionValue("time_limit", highs_.getRunTime() + std::max(0.0, timeout_s));
    highs_.setOptionValue("output_flag", verbose);

    requested_threads_ = threads;
    highs_.setOptionValue("threads", static_cast<HighsInt>(threads));
}

void HighsMasterSolver::restore_upper_bounds() {
    if (!binary_bounds_) {
        return;
    }
    const HighsLp& model = highs_.getLp();
    std::vector<double> lower;
    const std::span<const HighsInt> generated = columns_.all();
    for (const HighsInt index : generated) {
        lower.push_back(model.col_lower_[index]);
    }
    highs_.changeColsBounds(static_cast<HighsInt>(generated.size()),
                            generated.data(),
                            lower.data(),
                            upper_bounds_.data());
    binary_bounds_ = false;
}

HighsModelStatus HighsMasterSolver::run_once() {
    // A refused run leaves the previous solve's status in place.
    if (highs_.run() == HighsStatus::kError) {
        return HighsModelStatus::kSolveError;
    }
    return highs_.getModelStatus();
}

SolverStatus HighsMasterSolver::run(bool integer) {
    HighsModelStatus status = run_once();

    // HiGHS fixes its thread count at its first solve in the process and refuses a run asking
    // for another; 0 runs with the count in force.
    if (status == HighsModelStatus::kSolveError && requested_threads_ > 0) {
        if (warned_threads_ != requested_threads_) {
            warned_threads_ = requested_threads_;
            LOG_WARN("HighsMasterSolver: HiGHS keeps the thread count of its first solve in this ",
                     "process; this solve asked for ",
                     requested_threads_,
                     " threads and runs with that count instead\n");
        }
        highs_.setOptionValue("threads", HighsInt{0});
        status = run_once();
    }

    // After many rounds of added columns, simplex can wrongly detect cycling and stop with an
    // unknown status; the known way out is to start again from the basis on a cleared solver.
    if (!integer && (status == HighsModelStatus::kUnknown || status == HighsModelStatus::kNotset)) {
        const HighsBasis basis = highs_.getBasis();
        highs_.clearSolver();
        if (basis.valid) {
            highs_.setBasis(basis);
        }
        status = run_once();
    }

    if (status == HighsModelStatus::kUnboundedOrInfeasible) {
        const TemporaryOption presolve(highs_, "presolve", "off");
        status = run_once();
    }
    return status_of(status);
}

}  // namespace rcspp::column_generation
