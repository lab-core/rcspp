// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The HiGHS master solver from Python (USE_HIGHS): a HiGHS model the binding owns, filled from
// arrays, so the whole run stays in C++.

#include <Highs.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/types.hpp"
#include "rcspp_backends/highs/highs_master_solver.hpp"

namespace py = pybind11;

using namespace rcspp::column_generation;

namespace {

template <typename Value>
using InputArray = py::array_t<Value, py::array::c_style | py::array::forcecast>;

// The values of a one-dimensional array; `what` names it in the error.
template <typename Value>
std::vector<Value> to_vector(const InputArray<Value>& array, const std::string& what) {
    if (array.ndim() != 1) {
        throw std::invalid_argument("HighsMasterSolver: " + what +
                                    " must be a one-dimensional array");
    }
    return std::vector<Value>(array.data(), array.data() + array.size());
}

// The model as HiGHS takes it, checked so that HiGHS never reads past an array.
HighsLp make_model(const InputArray<double>& column_cost, const InputArray<double>& column_lower,
                   const InputArray<double>& column_upper, const InputArray<bool>& integer,
                   const InputArray<double>& row_lower, const InputArray<double>& row_upper,
                   const InputArray<std::int64_t>& starts,
                   const InputArray<std::int64_t>& row_indices, const InputArray<double>& values,
                   bool maximize, double offset) {
    HighsLp model;
    model.col_cost_ = to_vector(column_cost, "column_cost");
    model.col_lower_ = to_vector(column_lower, "column_lower");
    model.col_upper_ = to_vector(column_upper, "column_upper");
    model.row_lower_ = to_vector(row_lower, "row_lower");
    model.row_upper_ = to_vector(row_upper, "row_upper");
    const size_t columns = model.col_cost_.size();
    const size_t rows = model.row_lower_.size();
    const std::vector<bool> integer_columns = to_vector(integer, "integer");
    const std::vector<std::int64_t> start_values = to_vector(starts, "starts");
    const std::vector<std::int64_t> index_values = to_vector(row_indices, "row_indices");
    model.a_matrix_.value_ = to_vector(values, "values");
    if (model.col_lower_.size() != columns || model.col_upper_.size() != columns ||
        (!integer_columns.empty() && integer_columns.size() != columns) ||
        model.row_upper_.size() != rows || start_values.size() != columns + 1 ||
        index_values.size() != model.a_matrix_.value_.size()) {
        throw std::invalid_argument(
            "HighsMasterSolver: column_cost, column_lower, column_upper and integer (when given) "
            "need one value per column, row_lower and row_upper one per row, starts one more "
            "than the columns, and values one per row index");
    }
    if (start_values.front() != 0 || std::cmp_not_equal(start_values.back(), index_values.size()) ||
        !std::ranges::is_sorted(start_values)) {
        throw std::invalid_argument(
            "HighsMasterSolver: starts must rise from 0 to the number of entries");
    }
    model.num_col_ = static_cast<HighsInt>(columns);
    model.num_row_ = static_cast<HighsInt>(rows);
    model.a_matrix_.format_ = MatrixFormat::kColwise;
    model.a_matrix_.num_col_ = model.num_col_;
    model.a_matrix_.num_row_ = model.num_row_;
    model.a_matrix_.start_.assign(start_values.begin(), start_values.end());
    model.a_matrix_.index_.assign(index_values.begin(), index_values.end());
    if (!integer_columns.empty()) {
        model.integrality_.reserve(columns);
        for (const bool is_integer : integer_columns) {
            model.integrality_.push_back(is_integer ? HighsVarType::kInteger
                                                    : HighsVarType::kContinuous);
        }
    }
    model.sense_ = maximize ? ObjSense::kMaximize : ObjSense::kMinimize;
    model.offset_ = offset;
    return model;
}

// A HiGHS model holding `model`, quiet outside the solves (each solve sets its own output).
std::unique_ptr<Highs> make_highs(HighsLp model) {
    auto highs = std::make_unique<Highs>();
    highs->setOptionValue("output_flag", false);
    if (highs->passModel(std::move(model)) == HighsStatus::kError) {
        throw std::invalid_argument("HighsMasterSolver: HiGHS refused the model");
    }
    return highs;
}

// Every row of `highs`, the default rows that columns enter.
std::vector<HighsInt> every_row(const Highs& highs) {
    std::vector<HighsInt> rows(static_cast<size_t>(highs.getNumRow()));
    std::iota(rows.begin(), rows.end(), HighsInt{0});
    return rows;
}

// The C++ HighsMasterSolver over a HiGHS model it owns.
class OwnedHighsMasterSolver final : public MasterSolver {
    public:
        OwnedHighsMasterSolver(HighsLp model, std::optional<std::vector<HighsInt>> rows)
            : highs_(make_highs(std::move(model))),
              solver_(*highs_, rows ? std::move(*rows) : every_row(*highs_)) {}

        [[nodiscard]] size_t number_of_rows() const override { return solver_.number_of_rows(); }

        void row_bounds(std::span<double> lower, std::span<double> upper) const override {
            solver_.row_bounds(lower, upper);
        }

        void add_rows(const RowBatch& rows) override { solver_.add_rows(rows); }

        void add_columns(const ColumnBatch& columns) override { solver_.add_columns(columns); }

        SolverStatus solve_relaxation(const RelaxationParams& params) override {
            return solver_.solve_relaxation(params);
        }

        [[nodiscard]] double objective() const override { return solver_.objective(); }

        void duals(std::span<double> out) const override { solver_.duals(out); }

        void primal(std::span<const ColumnId> ids, std::span<double> out) const override {
            solver_.primal(ids, out);
        }

        SolverStatus solve_integer(const IntegerParams& params) override {
            return solver_.solve_integer(params);
        }

        [[nodiscard]] bool farkas(std::span<double> out) const override {
            return solver_.farkas(out);
        }

        void set_option(std::string_view name, const OptionValue& value) override {
            solver_.set_option(name, value);
        }

        [[nodiscard]] Capabilities capabilities() const override { return solver_.capabilities(); }

        [[nodiscard]] double dual_tolerance() const override { return solver_.dual_tolerance(); }

        [[nodiscard]] double largest_user_cost() const override {
            return solver_.largest_user_cost();
        }

        void restore_user_model() override { solver_.restore_user_model(); }

        // The HiGHS column of column `id`.
        [[nodiscard]] HighsInt column(ColumnId id) const { return solver_.column(id); }

    private:
        std::unique_ptr<Highs> highs_;
        HighsMasterSolver solver_;
};

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

void init_column_generation_highs(py::module_& m) {
    py::class_<OwnedHighsMasterSolver, MasterSolver, py::smart_holder>(
        m,
        "HighsMasterSolver",
        "A master solver over a HiGHS model it owns, built from arrays: empty by default, or "
        "the user's model, with its columns in compressed sparse column form (column j has "
        "the entries row_indices[starts[j]:starts[j + 1]] with the matching values). rows "
        "are the rows that pricing columns enter, in master row order; every row by default. "
        "The run stays in C++. A maximizing model is seen as a minimization. Options are set "
        "by HiGHS name with set_option; each solve sets time_limit, threads, output_flag and "
        "solve_relaxation (and mip_rel_gap) from its parameters and puts the values back.")
        .def(py::init([](const InputArray<double>& column_cost,
                         const InputArray<double>& column_lower,
                         const InputArray<double>& column_upper,
                         const InputArray<bool>& integer,
                         const InputArray<double>& row_lower,
                         const InputArray<double>& row_upper,
                         const InputArray<std::int64_t>& starts,
                         const InputArray<std::int64_t>& row_indices,
                         const InputArray<double>& values,
                         std::optional<std::vector<HighsInt>>
                             rows,
                         bool maximize,
                         double offset) {
                 return std::make_unique<OwnedHighsMasterSolver>(make_model(column_cost,
                                                                            column_lower,
                                                                            column_upper,
                                                                            integer,
                                                                            row_lower,
                                                                            row_upper,
                                                                            starts,
                                                                            row_indices,
                                                                            values,
                                                                            maximize,
                                                                            offset),
                                                                 std::move(rows));
             }),
             py::kw_only(),
             py::arg("column_cost") = std::vector<double>{},
             py::arg("column_lower") = std::vector<double>{},
             py::arg("column_upper") = std::vector<double>{},
             py::arg("integer") = std::vector<bool>{},
             py::arg("row_lower") = std::vector<double>{},
             py::arg("row_upper") = std::vector<double>{},
             py::arg("starts") = std::vector<std::int64_t>{0},
             py::arg("row_indices") = std::vector<std::int64_t>{},
             py::arg("values") = std::vector<double>{},
             py::arg("rows") = py::none(),
             py::arg("maximize") = false,
             py::arg("offset") = 0.0)
        .def("column",
             &OwnedHighsMasterSolver::column,
             py::arg("id"),
             "The HiGHS column of column id.");
}
