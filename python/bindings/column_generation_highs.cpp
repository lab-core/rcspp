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

        // The HiGHS model.
        [[nodiscard]] const Highs& highs() const { return *highs_; }

    private:
        std::unique_ptr<Highs> highs_;
        HighsMasterSolver solver_;
};

// The cost, bounds and entries of columns `ids` in the HiGHS model of `solver`, the entries in
// compressed sparse column form over the model's rows.
py::tuple model_columns(const OwnedHighsMasterSolver& solver, const InputArray<ColumnId>& ids) {
    const std::vector<ColumnId> id_values = to_vector(ids, "ids");
    const Highs& highs = solver.highs();
    const auto count = static_cast<py::ssize_t>(id_values.size());
    py::array_t<double> cost(count);
    py::array_t<double> lower(count);
    py::array_t<double> upper(count);
    std::vector<std::int64_t> starts{0};
    std::vector<std::int64_t> row_indices;
    std::vector<double> values;

    // A column has at most one entry per row.
    std::vector<HighsInt> column_rows(static_cast<size_t>(highs.getNumRow()));
    std::vector<double> column_values(column_rows.size());
    for (size_t index = 0; index < id_values.size(); ++index) {
        const HighsInt column = solver.column(id_values[index]);
        HighsInt got = 0;
        HighsInt entries = 0;
        HighsInt start = 0;
        highs.getCols(column,
                      column,
                      got,
                      cost.mutable_data() + index,
                      lower.mutable_data() + index,
                      upper.mutable_data() + index,
                      entries,
                      &start,
                      column_rows.data(),
                      column_values.data());

        row_indices.insert(row_indices.end(), column_rows.begin(), column_rows.begin() + entries);
        values.insert(values.end(), column_values.begin(), column_values.begin() + entries);
        starts.push_back(static_cast<std::int64_t>(row_indices.size()));
    }

    return py::make_tuple(
        cost,
        lower,
        upper,
        py::array_t<std::int64_t>(static_cast<py::ssize_t>(starts.size()), starts.data()),
        py::array_t<std::int64_t>(static_cast<py::ssize_t>(row_indices.size()), row_indices.data()),
        py::array_t<double>(static_cast<py::ssize_t>(values.size()), values.data()));
}

// The bounds and names of the rows of the HiGHS model of `solver` from row `first` on.
py::tuple model_rows(const OwnedHighsMasterSolver& solver, HighsInt first) {
    const HighsLp& model = solver.highs().getLp();
    if (first < 0 || first > model.num_row_) {
        throw std::out_of_range(
            "HighsMasterSolver.model_rows: first must be between 0 and the "
            "number of rows of the model");
    }

    const auto begin = static_cast<size_t>(first);
    const auto end = static_cast<size_t>(model.num_row_);
    const auto count = static_cast<py::ssize_t>(end - begin);
    std::vector<std::string> names;
    names.reserve(end - begin);
    for (size_t row = begin; row < end; ++row) {
        names.push_back(row < model.row_names_.size() ? model.row_names_[row] : std::string());
    }
    return py::make_tuple(py::array_t<double>(count, model.row_lower_.data() + begin),
                          py::array_t<double>(count, model.row_upper_.data() + begin),
                          names);
}

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
             "The HiGHS column of column id.")
        .def("model_columns",
             &model_columns,
             py::arg("ids"),
             "The columns ids as the HiGHS model holds them: (cost, lower, upper, starts, "
             "row_indices, values), the entries in compressed sparse column form over the "
             "model's rows, and the costs in the model's own sense.")
        .def("model_rows",
             &model_rows,
             py::arg("first") = 0,
             "The rows of the HiGHS model from row first on: (lower, upper, names), an unnamed "
             "row's name empty. The rows added during a run come after the model's own.");
}
