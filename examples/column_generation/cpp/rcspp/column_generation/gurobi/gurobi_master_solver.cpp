// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/gurobi/gurobi_master_solver.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace rcspp::column_generation {

namespace {

constexpr double infinity = std::numeric_limits<double>::infinity();

// Gurobi writes infinity as ±GRB_INFINITY (1e100).
double to_gurobi(double value) {
    return std::clamp(value, -GRB_INFINITY, GRB_INFINITY);
}

double from_gurobi(double value) {
    if (value >= GRB_INFINITY) {
        return infinity;
    }
    if (value <= -GRB_INFINITY) {
        return -infinity;
    }
    return value;
}

// Gurobi returns attribute arrays allocated with new[]: copies one into a vector and frees it.
template <typename Value>
std::vector<Value> take(Value* array, size_t count) {
    std::vector<Value> values(array, array + count);
    delete[] array;
    return values;
}

SolverStatus status_of(int status) {
    switch (status) {
        case GRB_OPTIMAL:
            return SolverStatus::Optimal;
        case GRB_INFEASIBLE:
            return SolverStatus::Infeasible;
        case GRB_UNBOUNDED:
            return SolverStatus::Unbounded;
        case GRB_TIME_LIMIT:
            return SolverStatus::TimeLimit;
        default:
            return SolverStatus::Other;
    }
}

// A row as Gurobi holds it: `a·x (sense) side`.
struct GurobiRow {
        char sense;
        double side;
};

// The Gurobi row for `lower ≤ a·x ≤ upper`: `=` for equal finite bounds, `≥` or `≤` for one
// finite bound, `≤ ∞` for a free row.
GurobiRow gurobi_row(double lower, double upper) {
    if (!row_bounds_can_be_held(lower, upper)) {
        throw std::invalid_argument(
            "GurobiMasterSolver::add_rows: row bounds must be numbers, with lower < +inf and "
            "upper > -inf");
    }

    const bool finite_lower = std::isfinite(lower);
    const bool finite_upper = std::isfinite(upper);
    if (finite_lower && finite_upper) {
        if (lower != upper) {
            throw std::invalid_argument(
                "GurobiMasterSolver::add_rows: a row with two different finite bounds is a range; "
                "build it in the model instead");
        }
        return {.sense = GRB_EQUAL, .side = lower};
    }
    if (finite_lower) {
        return {.sense = GRB_GREATER_EQUAL, .side = lower};
    }
    if (finite_upper) {
        return {.sense = GRB_LESS_EQUAL, .side = upper};
    }
    return {.sense = GRB_LESS_EQUAL, .side = GRB_INFINITY};
}

// Sets a parameter for the lifetime of the guard, then puts the old value back.
template <typename Parameter, typename Value>
class TemporaryParameter {
    public:
        TemporaryParameter(GRBModel& model, Parameter parameter, Value value)
            : model_(model), parameter_(parameter), old_value_(model.get(parameter)) {
            model_.set(parameter_, value);
        }

        TemporaryParameter(const TemporaryParameter&) = delete;
        TemporaryParameter& operator=(const TemporaryParameter&) = delete;
        TemporaryParameter(TemporaryParameter&&) = delete;
        TemporaryParameter& operator=(TemporaryParameter&&) = delete;

        ~TemporaryParameter() {
            try {
                model_.set(parameter_, old_value_);
            } catch (const GRBException&) {  // a destructor must not throw
            }
        }

    private:
        GRBModel& model_;
        Parameter parameter_;
        Value old_value_;
};

// The options every solve sets from its parameters, for the lifetime of the solve: the user's
// values come back after it.
class SolveOptions {
    public:
        SolveOptions(GRBModel& model, double timeout_s, int threads, bool verbose)
            : time_limit_(model, GRB_DoubleParam_TimeLimit, to_gurobi(std::max(0.0, timeout_s))),
              threads_(model, GRB_IntParam_Threads, threads),
              output_(model, GRB_IntParam_OutputFlag, verbose ? 1 : 0) {}

    private:
        TemporaryParameter<GRB_DoubleParam, double> time_limit_;
        TemporaryParameter<GRB_IntParam, int> threads_;
        TemporaryParameter<GRB_IntParam, int> output_;
};

}  // namespace

GurobiMasterSolver::GurobiMasterSolver(GRBModel& model, std::vector<GRBConstr> rows)
    : model_(model), rows_(std::move(rows)) {
    // Rows created just before are pending until an update; their attributes are read below.
    model_.update();
}

void GurobiMasterSolver::row_bounds(std::span<double> lower, std::span<double> upper) const {
    if (rows_.empty()) {
        return;
    }

    const int count = static_cast<int>(rows_.size());
    const std::vector<char> senses =
        take(model_.get(GRB_CharAttr_Sense, rows_.data(), count), rows_.size());
    const std::vector<double> sides =
        take(model_.get(GRB_DoubleAttr_RHS, rows_.data(), count), rows_.size());

    for (size_t row = 0; row < rows_.size(); ++row) {
        const double side = from_gurobi(sides[row]);
        lower[row] = senses[row] == GRB_LESS_EQUAL ? -infinity : side;
        upper[row] = senses[row] == GRB_GREATER_EQUAL ? infinity : side;
    }
}

void GurobiMasterSolver::add_rows(const RowBatch& rows) {
    // Convert every row first, so that a refused batch adds nothing.
    std::vector<GurobiRow> converted;
    converted.reserve(rows.lower.size());
    for (size_t row = 0; row < rows.lower.size(); ++row) {
        converted.push_back(gurobi_row(rows.lower[row], rows.upper[row]));
    }

    for (size_t row = 0; row < converted.size(); ++row) {
        const std::string name = rows.names.empty() ? std::string() : rows.names[row];
        rows_.push_back(
            model_.addConstr(GRBLinExpr(), converted[row].sense, converted[row].side, name));
    }

    model_.update();
}

void GurobiMasterSolver::add_columns(const ColumnBatch& columns) {
    columns_.check(columns, rows_.size());

    const double cost_sign = sense_sign();
    std::vector<GRBConstr> constraints;

    for (size_t column = 0; column < columns.size(); ++column) {
        const ColumnId id = columns.ids[column];

        // The column's entries.
        const std::uint32_t begin = columns.starts[column];
        const std::uint32_t end = columns.starts[column + 1];
        constraints.clear();
        for (std::uint32_t entry = begin; entry < end; ++entry) {
            constraints.push_back(rows_[columns.row_indices[entry]]);
        }

        GRBColumn entries;
        if (end > begin) {
            entries.addTerms(&columns.values[begin],
                             constraints.data(),
                             static_cast<int>(constraints.size()));
        }

        const GRBVar added = model_.addVar(to_gurobi(columns.lower[column]),
                                           to_gurobi(columns.upper[column]),
                                           cost_sign * columns.cost[column],
                                           GRB_CONTINUOUS,
                                           entries);
        columns_.add(id, added);
    }

    model_.update();
}

SolverStatus GurobiMasterSolver::solve_relaxation(const RelaxationParams& params) {
    relax();
    const SolveOptions options(model_, params.timeout_s, params.threads, params.verbose);

    // After columns are added the last basis stays primal feasible, so primal simplex resumes
    // from it; a method the user chose is kept.
    if (model_.get(GRB_IntParam_Method) == GRB_METHOD_AUTO) {
        const TemporaryParameter method(model_, GRB_IntParam_Method, GRB_METHOD_PRIMAL);
        return optimize();
    }
    return optimize();
}

double GurobiMasterSolver::objective() const {
    return sense_sign() * model_.get(GRB_DoubleAttr_ObjVal);
}

void GurobiMasterSolver::duals(std::span<double> out) const {
    if (rows_.empty()) {
        return;
    }

    const double sign = sense_sign();
    const std::vector<double> values =
        take(model_.get(GRB_DoubleAttr_Pi, rows_.data(), static_cast<int>(rows_.size())),
             rows_.size());
    for (size_t row = 0; row < rows_.size(); ++row) {
        out[row] = sign * values[row];
    }
}

void GurobiMasterSolver::primal(std::span<const ColumnId> ids, std::span<double> out) const {
    if (ids.empty()) {
        return;
    }

    std::vector<GRBVar> variables;
    variables.reserve(ids.size());
    std::ranges::transform(ids, std::back_inserter(variables), [this](ColumnId id) {
        return variable(id);
    });

    const std::vector<double> values =
        take(model_.get(GRB_DoubleAttr_X, variables.data(), static_cast<int>(variables.size())),
             variables.size());
    std::ranges::copy(values, out.begin());
}

SolverStatus GurobiMasterSolver::solve_integer(const IntegerParams& params) {
    restore(params.column_type == ColumnType::Binary ? GRB_BINARY : GRB_INTEGER);
    const SolveOptions options(model_, params.timeout_s, params.threads, params.verbose);
    const TemporaryParameter gap(model_, GRB_DoubleParam_MIPGap, params.relative_gap);
    return optimize();
}

double GurobiMasterSolver::dual_tolerance() const {
    return model_.get(GRB_DoubleParam_OptimalityTol);
}

double GurobiMasterSolver::largest_user_cost() const {
    // Variables added just before are pending until an update.
    model_.update();
    const int count = model_.get(GRB_IntAttr_NumVars);
    if (count == 0) {
        return 0.0;
    }

    std::vector<bool> generated(static_cast<size_t>(count), false);
    for (const GRBVar& column : columns_.all()) {
        generated[static_cast<size_t>(column.index())] = true;
    }
    const std::vector<GRBVar> variables = take(model_.getVars(), static_cast<size_t>(count));
    const std::vector<double> costs =
        take(model_.get(GRB_DoubleAttr_Obj, variables.data(), count), static_cast<size_t>(count));

    double largest = 0.0;
    for (size_t index = 0; index < costs.size(); ++index) {
        if (!generated[index]) {
            largest = std::max(largest, std::abs(costs[index]));
        }
    }
    return largest;
}

void GurobiMasterSolver::restore_user_model() {
    // The generated columns stay, continuous as they were added. No update here: Gurobi applies
    // the types at the user's next update or optimize, and the last solve's values stay readable
    // until then.
    if (relaxed_) {
        restore(GRB_CONTINUOUS);
    }
}

void GurobiMasterSolver::set_option(std::string_view name, const OptionValue& value) {
    const std::string parameter(name);

    try {
        std::visit(
            [&](const auto& option) {
                using Value = std::decay_t<decltype(option)>;
                if constexpr (std::is_same_v<Value, bool>) {
                    model_.set(parameter, option ? "1" : "0");
                } else if constexpr (std::is_same_v<Value, std::string>) {
                    model_.set(parameter, option);
                } else {
                    // The shortest text that reads back as the same number.
                    model_.set(parameter, std::format("{}", option));
                }
            },
            value);
    } catch (const GRBException& error) {
        throw std::invalid_argument(
            "GurobiMasterSolver::set_option: Gurobi refused the parameter '" + parameter +
            "' or its value: " + error.getMessage());
    }
}

GRBVar GurobiMasterSolver::variable(ColumnId id) const {
    return columns_.at(id);
}

double GurobiMasterSolver::sense_sign() const {
    return model_.get(GRB_IntAttr_ModelSense) == GRB_MAXIMIZE ? -1.0 : 1.0;
}

void GurobiMasterSolver::relax() {
    const size_t known = user_types_.size();
    record_new_user_types();

    // Relaxed already: only the user's variables added since are not.
    std::vector<GRBVar> variables;
    std::vector<GRBVar> binaries;
    if (!relaxed_) {
        variables.assign(columns_.all().begin(), columns_.all().end());
    }
    for (size_t index = relaxed_ ? known : 0; index < user_types_.size(); ++index) {
        variables.push_back(user_types_[index].first);
        if (user_types_[index].second == GRB_BINARY) {
            binaries.push_back(user_types_[index].first);
        }
    }
    relaxed_ = true;
    if (variables.empty()) {
        return;
    }

    const std::vector<char> types(variables.size(), GRB_CONTINUOUS);
    model_.set(GRB_CharAttr_VType,
               variables.data(),
               types.data(),
               static_cast<int>(variables.size()));
    bound_binaries(binaries);
}

void GurobiMasterSolver::bound_binaries(std::vector<GRBVar>& binaries) {
    if (binaries.empty()) {
        return;
    }

    // Gurobi keeps a binary within 0 and 1 by its type alone, whatever bounds the user set after
    // creating it: once continuous, it needs them as bounds.
    const int count = static_cast<int>(binaries.size());
    std::vector<double> lower =
        take(model_.get(GRB_DoubleAttr_LB, binaries.data(), count), binaries.size());
    std::vector<double> upper =
        take(model_.get(GRB_DoubleAttr_UB, binaries.data(), count), binaries.size());
    for (size_t index = 0; index < binaries.size(); ++index) {
        relaxed_binaries_.push_back(
            {.variable = binaries[index], .lower = lower[index], .upper = upper[index]});
        lower[index] = std::max(lower[index], 0.0);
        upper[index] = std::min(upper[index], 1.0);
    }
    model_.set(GRB_DoubleAttr_LB, binaries.data(), lower.data(), count);
    model_.set(GRB_DoubleAttr_UB, binaries.data(), upper.data(), count);
}

void GurobiMasterSolver::restore(char type) {
    record_new_user_types();

    std::vector<GRBVar> variables(columns_.all().begin(), columns_.all().end());
    std::vector<char> types(variables.size(), type);
    for (const auto& [user_variable, user_type] : user_types_) {
        variables.push_back(user_variable);
        types.push_back(user_type);
    }
    model_.set(GRB_CharAttr_VType,
               variables.data(),
               types.data(),
               static_cast<int>(variables.size()));

    // The user's binaries get their own bounds back.
    for (const RelaxedBinary& binary : relaxed_binaries_) {
        GRBVar variable = binary.variable;
        variable.set(GRB_DoubleAttr_LB, binary.lower);
        variable.set(GRB_DoubleAttr_UB, binary.upper);
    }
    relaxed_binaries_.clear();
    relaxed_ = false;
}

void GurobiMasterSolver::record_new_user_types() {
    // Variables the user added are pending until an update.
    model_.update();
    const auto count = static_cast<size_t>(model_.get(GRB_IntAttr_NumVars));
    if (count <= recorded_variables_) {
        return;
    }

    // A generated column is continuous whenever this runs: added so, and made integer only by
    // the integer solve, after this. Every other type belongs to a variable of the user.
    std::vector<GRBVar> variables;
    for (size_t index = recorded_variables_; index < count; ++index) {
        variables.push_back(model_.getVar(static_cast<int>(index)));
    }
    const std::vector<char> types =
        take(model_.get(GRB_CharAttr_VType, variables.data(), static_cast<int>(variables.size())),
             variables.size());
    for (size_t index = 0; index < variables.size(); ++index) {
        if (types[index] != GRB_CONTINUOUS) {
            user_types_.emplace_back(variables[index], types[index]);
        }
    }
    recorded_variables_ = count;
}

SolverStatus GurobiMasterSolver::optimize() {
    model_.optimize();
    int status = model_.get(GRB_IntAttr_Status);

    if (status == GRB_INF_OR_UNBD) {
        // The second solve gets what is left of the time limit, not a limit of its own.
        const double time_left =
            std::max(0.0,
                     model_.get(GRB_DoubleParam_TimeLimit) - model_.get(GRB_DoubleAttr_Runtime));
        const TemporaryParameter limit(model_, GRB_DoubleParam_TimeLimit, time_left);
        const TemporaryParameter reductions(model_, GRB_IntParam_DualReductions, 0);
        model_.optimize();
        status = model_.get(GRB_IntAttr_Status);
    }
    return status_of(status);
}

}  // namespace rcspp::column_generation
