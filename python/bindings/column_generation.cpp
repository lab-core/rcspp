// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "rcspp/column_generation/column_generation.hpp"

#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "active_call.hpp"
#include "graph_pricer.hpp"
#include "rcspp/column_generation/master_initialization.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/master_solver_self_test.hpp"
#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/statistics.hpp"
#include "rcspp/column_generation/tolerances.hpp"
#include "rcspp/column_generation/types.hpp"

using namespace rcspp;
using namespace rcspp::column_generation;

namespace {

// ─── Arrays ──────────────────────────────────────────────────────────────────
// Every array crossing the boundary is a numpy copy: a view of a C++ span would dangle once the
// call that lent it returns.

// An array argument: anything numpy can turn into a contiguous array of Value.
template <typename Value>
using InputArray = py::array_t<Value, py::array::c_style | py::array::forcecast>;

// A numpy copy of `values`.
template <typename Value>
py::array_t<Value> to_array(std::span<const Value> values) {
    py::array_t<Value> array(static_cast<py::ssize_t>(values.size()));
    std::ranges::copy(values, array.mutable_data());
    return array;
}

// A span over a one-dimensional array; `what` names it in the error.
template <typename Value>
std::span<const Value> to_span(const InputArray<Value>& array, const std::string& what) {
    if (array.ndim() != 1) {
        throw std::invalid_argument(what + ": expected a one-dimensional array");
    }
    return {array.data(), static_cast<size_t>(array.size())};
}

// Copies the values a Python method returned into `out`, which they must fill exactly.
void copy_returned(const py::handle& returned, std::span<double> out, const std::string& method) {
    const auto values = InputArray<double>::ensure(returned);
    if (!values) {
        throw py::type_error(method + ": expected an array of numbers");
    }
    const std::span<const double> view = to_span(values, method);
    if (view.size() != out.size()) {
        throw std::invalid_argument(method + ": expected " + std::to_string(out.size()) +
                                    " values, got " + std::to_string(view.size()));
    }
    std::ranges::copy(view, out.begin());
}

// ─── Batches that own their arrays ───────────────────────────────────────────
// What a Python master solver receives, and what Python gives a master solver.

struct RowBatchArrays {
        InputArray<double> lower;
        InputArray<double> upper;
        std::vector<std::string> names;

        static RowBatchArrays copy(const RowBatch& rows) {
            return {.lower = to_array(rows.lower),
                    .upper = to_array(rows.upper),
                    .names = {rows.names.begin(), rows.names.end()}};
        }

        [[nodiscard]] RowBatch view() const {
            const auto lower_view = to_span(lower, "RowBatch.lower");
            const auto upper_view = to_span(upper, "RowBatch.upper");
            if (upper_view.size() != lower_view.size() ||
                (!names.empty() && names.size() != lower_view.size())) {
                throw std::invalid_argument(
                    "RowBatch: lower, upper and names (when given) must have the same size");
            }
            return {.lower = lower_view, .upper = upper_view, .names = names};
        }
};

struct ColumnBatchArrays {
        InputArray<ColumnId> ids;
        InputArray<double> cost;
        InputArray<double> lower;
        InputArray<double> upper;
        InputArray<std::uint32_t> starts;
        InputArray<std::uint32_t> row_indices;
        InputArray<double> values;

        static ColumnBatchArrays copy(const ColumnBatch& columns) {
            return {.ids = to_array(columns.ids),
                    .cost = to_array(columns.cost),
                    .lower = to_array(columns.lower),
                    .upper = to_array(columns.upper),
                    .starts = to_array(columns.starts),
                    .row_indices = to_array(columns.row_indices),
                    .values = to_array(columns.values)};
        }

        // The batch as C++ sees it, checked so that no master solver reads past an array.
        [[nodiscard]] ColumnBatch view() const {
            const ColumnBatch batch{.ids = to_span(ids, "ColumnBatch.ids"),
                                    .cost = to_span(cost, "ColumnBatch.cost"),
                                    .lower = to_span(lower, "ColumnBatch.lower"),
                                    .upper = to_span(upper, "ColumnBatch.upper"),
                                    .starts = to_span(starts, "ColumnBatch.starts"),
                                    .row_indices = to_span(row_indices, "ColumnBatch.row_indices"),
                                    .values = to_span(values, "ColumnBatch.values")};
            const size_t size = batch.size();
            if (batch.cost.size() != size || batch.lower.size() != size ||
                batch.upper.size() != size || batch.starts.size() != size + 1 ||
                batch.values.size() != batch.row_indices.size()) {
                throw std::invalid_argument(
                    "ColumnBatch: cost, lower and upper need one value per id, starts one more, "
                    "and values one per row index");
            }
            if (batch.starts.front() != 0 || batch.starts.back() != batch.row_indices.size() ||
                !std::ranges::is_sorted(batch.starts)) {
                throw std::invalid_argument(
                    "ColumnBatch: starts must rise from 0 to the number of entries");
            }
            return batch;
        }
};

// ─── Trampolines ─────────────────────────────────────────────────────────────
// A Python subclass overrides the methods with the signatures of the bound ones: arrays are
// returned, not written into a buffer.

// The Python override of `name`, which must exist: fails as pybind11 does for a pure virtual
// method. The caller holds the GIL (PYBIND11_OVERRIDE_PURE fails after releasing it, and a debug
// build then asserts on the Python error state without it).
template <typename Base>
py::function required_override(const Base* self, const char* class_name, const char* name) {
    py::function function = py::get_override(self, name);
    if (!function) {
        pybind11::pybind11_fail(std::string("Tried to call pure virtual function \"") + class_name +
                                "::" + name + "\"");
    }
    return function;
}

// Calls the Python override `name`, which must exist, with C++ arguments converted under the GIL.
template <typename Result, typename Base, typename... Args>
Result call_required(const Base* self, const char* class_name, const char* name, Args&&... args) {
    const py::gil_scoped_acquire gil;
    py::object result = required_override(self, class_name, name)(std::forward<Args>(args)...);
    if constexpr (!std::is_void_v<Result>) {
        return result.template cast<Result>();
    }
}

class PythonMasterSolver : public MasterSolver, public py::trampoline_self_life_support {
    public:
        [[nodiscard]] size_t number_of_rows() const override {
            return call_required<size_t>(this, "MasterSolver", "number_of_rows");
        }

        void row_bounds(std::span<double> lower, std::span<double> upper) const override {
            const py::gil_scoped_acquire gil;
            const py::tuple bounds(required_override(this, "MasterSolver", "row_bounds")());
            if (bounds.size() != 2) {
                throw std::invalid_argument("MasterSolver.row_bounds: expected (lower, upper)");
            }
            copy_returned(bounds[0], lower, "MasterSolver.row_bounds");
            copy_returned(bounds[1], upper, "MasterSolver.row_bounds");
        }

        void add_rows(const RowBatch& rows) override {
            {
                const py::gil_scoped_acquire gil;
                if (const py::function function = py::get_override(this, "add_rows")) {
                    function(RowBatchArrays::copy(rows));
                    return;
                }
            }
            MasterSolver::add_rows(rows);
        }

        void add_columns(const ColumnBatch& columns) override {
            const py::gil_scoped_acquire gil;
            required_override(this, "MasterSolver", "add_columns")(
                ColumnBatchArrays::copy(columns));
        }

        SolverStatus solve_relaxation(const RelaxationParams& params) override {
            return call_required<SolverStatus>(this,
                                               "MasterSolver",
                                               "solve_relaxation",
                                               RelaxationParams(params));
        }

        [[nodiscard]] double objective() const override {
            return call_required<double>(this, "MasterSolver", "objective");
        }

        void duals(std::span<double> out) const override {
            const py::gil_scoped_acquire gil;
            copy_returned(required_override(this, "MasterSolver", "duals")(),
                          out,
                          "MasterSolver.duals");
        }

        void primal(std::span<const ColumnId> ids, std::span<double> out) const override {
            const py::gil_scoped_acquire gil;
            copy_returned(required_override(this, "MasterSolver", "primal")(to_array(ids)),
                          out,
                          "MasterSolver.primal");
        }

        SolverStatus solve_integer(const IntegerParams& params) override {
            return call_required<SolverStatus>(this,
                                               "MasterSolver",
                                               "solve_integer",
                                               IntegerParams(params));
        }

        [[nodiscard]] bool farkas(std::span<double> out) const override {
            {
                const py::gil_scoped_acquire gil;
                if (const py::function function = py::get_override(this, "farkas")) {
                    const py::object ray = function();
                    if (ray.is_none()) {
                        return false;
                    }
                    copy_returned(ray, out, "MasterSolver.farkas");
                    return true;
                }
            }
            return MasterSolver::farkas(out);
        }

        void set_option(std::string_view name, const OptionValue& value) override {
            call_required<void>(this, "MasterSolver", "set_option", std::string(name), value);
        }

        [[nodiscard]] Capabilities capabilities() const override {
            return call_required<Capabilities>(this, "MasterSolver", "capabilities");
        }

        [[nodiscard]] double dual_tolerance() const override {
            PYBIND11_OVERRIDE(double, MasterSolver, dual_tolerance, );
        }

        [[nodiscard]] double largest_user_cost() const override {
            PYBIND11_OVERRIDE(double, MasterSolver, largest_user_cost, );
        }

        void restore_user_model() override {
            PYBIND11_OVERRIDE(void, MasterSolver, restore_user_model, );
        }
};

class PythonPricer : public Pricer, public py::trampoline_self_life_support {
    public:
        PricingResult price(std::span<const double> duals, const PricingRequest& request) override {
            const py::gil_scoped_acquire gil;
            return required_override(this, "Pricer", "price")(to_array(duals),
                                                              PricingRequest(request))
                .cast<PricingResult>();
        }

        void update(const PricingChanges& changes) override {
            PYBIND11_OVERRIDE(void, Pricer, update, PricingChanges(changes));
        }

        [[nodiscard]] double cost_scale() const override {
            PYBIND11_OVERRIDE(double, Pricer, cost_scale, );
        }
};

class PythonTolerances : public Tolerances, public py::trampoline_self_life_support {
    public:
        [[nodiscard]] double reduced_cost(const ToleranceContext& context) const override {
            return call_required<double>(this,
                                         "Tolerances",
                                         "reduced_cost",
                                         ToleranceContext(context));
        }

        [[nodiscard]] double feasibility(const ToleranceContext& context) const override {
            return call_required<double>(this,
                                         "Tolerances",
                                         "feasibility",
                                         ToleranceContext(context));
        }

        [[nodiscard]] double optimality_gap(const ToleranceContext& context) const override {
            return call_required<double>(this,
                                         "Tolerances",
                                         "optimality_gap",
                                         ToleranceContext(context));
        }
};

class PythonMasterInitialization : public MasterInitialization,
                                   public py::trampoline_self_life_support {
    public:
        // The master goes by pointer: Python gets the master's own object, not a copy.
        void initialize(MasterProblem& master, const ToleranceContext& context) override {
            call_required<void>(this,
                                "MasterInitialization",
                                "initialize",
                                &master,
                                ToleranceContext(context));
        }
};

// ─── Hooks ───────────────────────────────────────────────────────────────────

// Sets the iteration hook. The Python hook gets its own copy of each IterationInfo, so it may
// keep it; pybind11's wrapper takes the GIL for each call, and for each copy of the hook.
void set_iteration_hook(ColumnGenerationParams& params, std::function<void(IterationInfo)> hook) {
    if (!hook) {
        params.on_iteration = nullptr;
        return;
    }
    params.on_iteration = [hook = std::move(hook)](const IterationInfo& info) { hook(info); };
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

void init_column_generation(py::module_& m) {
    // ── Enums ─────────────────────────────────────────────────────────────────

    py::enum_<SolverStatus>(m, "SolverStatus", "Outcome of an LP or MIP solve by a MasterSolver.")
        .value("Optimal", SolverStatus::Optimal)
        .value("Infeasible", SolverStatus::Infeasible)
        .value("Unbounded", SolverStatus::Unbounded)
        .value("TimeLimit", SolverStatus::TimeLimit)
        .value("Other", SolverStatus::Other);

    py::enum_<RowSense>(m, "RowSense", "Sense of a master row: a·x = rhs, a·x ≥ rhs or a·x ≤ rhs.")
        .value("Equal", RowSense::Equal)
        .value("GreaterEqual", RowSense::GreaterEqual)
        .value("LessEqual", RowSense::LessEqual);

    py::enum_<ColumnType>(m, "ColumnType", "Type of the generated columns in the integer solve.")
        .value("Integer", ColumnType::Integer)
        .value("Binary", ColumnType::Binary);

    py::enum_<PricingStatus>(m, "PricingStatus", "How a pricing call ended.")
        .value("Optimal", PricingStatus::Optimal)
        .value("SolverLimit", PricingStatus::SolverLimit)
        .value("Infeasible", PricingStatus::Infeasible)
        .value("NotApplicable", PricingStatus::NotApplicable)
        .value("Unknown", PricingStatus::Unknown);

    py::enum_<ColumnTag>(m, "ColumnTag", "Where a master column comes from.")
        .value("Real", ColumnTag::Real)
        .value("Artificial", ColumnTag::Artificial);

    py::enum_<ConvergenceAction>(m, "ConvergenceAction", "What the convergence hook decides.")
        .value("Stop", ConvergenceAction::Stop)
        .value("Continue", ConvergenceAction::Continue);

    py::enum_<ColumnGenerationStatus>(m, "ColumnGenerationStatus", "How a run ended.")
        .value("Optimal", ColumnGenerationStatus::Optimal)
        .value("NotProven", ColumnGenerationStatus::NotProven)
        .value("Infeasible", ColumnGenerationStatus::Infeasible)
        .value("RestrictedMasterInfeasible", ColumnGenerationStatus::RestrictedMasterInfeasible)
        .value("Unbounded", ColumnGenerationStatus::Unbounded)
        .value("TimeLimit", ColumnGenerationStatus::TimeLimit)
        .value("IterationLimit", ColumnGenerationStatus::IterationLimit)
        .value("SolverError", ColumnGenerationStatus::SolverError);

    // ── Constants ─────────────────────────────────────────────────────────────

    m.attr("default_relative_gap") = default_relative_gap;
    m.attr("default_reduced_cost_tolerance") = default_reduced_cost_tolerance;
    m.attr("default_feasibility_tolerance") = default_feasibility_tolerance;
    m.attr("default_optimality_gap") = default_optimality_gap;

    // ── Options and results of the master solver ──────────────────────────────

    const Capabilities capabilities_defaults;
    py::class_<Capabilities>(m, "Capabilities", "What a master solver can do beyond the required.")
        .def(py::init([](bool add_rows, bool integer_solve, bool farkas, bool basis) {
                 return Capabilities{.add_rows = add_rows,
                                     .integer_solve = integer_solve,
                                     .farkas = farkas,
                                     .basis = basis};
             }),
             py::kw_only(),
             py::arg("add_rows") = capabilities_defaults.add_rows,
             py::arg("integer_solve") = capabilities_defaults.integer_solve,
             py::arg("farkas") = capabilities_defaults.farkas,
             py::arg("basis") = capabilities_defaults.basis)
        .def_readwrite("add_rows", &Capabilities::add_rows)
        .def_readwrite("integer_solve", &Capabilities::integer_solve)
        .def_readwrite("farkas", &Capabilities::farkas)
        .def_readwrite("basis", &Capabilities::basis);

    const RelaxationParams relaxation_defaults;
    py::class_<RelaxationParams>(m, "RelaxationParams", "Options of one LP solve.")
        .def(py::init([](double timeout_s, int threads, bool verbose) {
                 return RelaxationParams{.timeout_s = timeout_s,
                                         .threads = threads,
                                         .verbose = verbose};
             }),
             py::kw_only(),
             py::arg("timeout_s") = relaxation_defaults.timeout_s,
             py::arg("threads") = relaxation_defaults.threads,
             py::arg("verbose") = relaxation_defaults.verbose)
        .def_readwrite("timeout_s", &RelaxationParams::timeout_s)
        .def_readwrite("threads", &RelaxationParams::threads, "Number of threads; 0: the solver's.")
        .def_readwrite("verbose", &RelaxationParams::verbose);

    const IntegerParams integer_defaults;
    py::class_<IntegerParams>(m, "IntegerParams", "Options of the integer solve of the master.")
        .def(py::init([](double timeout_s,
                         double relative_gap,
                         ColumnType column_type,
                         int threads,
                         bool verbose) {
                 return IntegerParams{.timeout_s = timeout_s,
                                      .relative_gap = relative_gap,
                                      .column_type = column_type,
                                      .threads = threads,
                                      .verbose = verbose};
             }),
             py::kw_only(),
             py::arg("timeout_s") = integer_defaults.timeout_s,
             py::arg("relative_gap") = integer_defaults.relative_gap,
             py::arg("column_type") = integer_defaults.column_type,
             py::arg("threads") = integer_defaults.threads,
             py::arg("verbose") = integer_defaults.verbose)
        .def_readwrite("timeout_s", &IntegerParams::timeout_s)
        .def_readwrite("relative_gap", &IntegerParams::relative_gap)
        .def_readwrite("column_type", &IntegerParams::column_type)
        .def_readwrite("threads", &IntegerParams::threads)
        .def_readwrite("verbose", &IntegerParams::verbose);

    py::class_<RowBatchArrays>(m,
                               "RowBatch",
                               "Rows to add, as ranges lower ≤ a·x ≤ upper (numpy copies).")
        .def(py::init([](InputArray<double> lower,
                         InputArray<double>
                             upper,
                         std::vector<std::string>
                             names) {
                 RowBatchArrays rows{.lower = std::move(lower),
                                     .upper = std::move(upper),
                                     .names = std::move(names)};
                 static_cast<void>(rows.view());
                 return rows;
             }),
             py::arg("lower"),
             py::arg("upper"),
             py::arg("names") = std::vector<std::string>{})
        .def_readonly("lower", &RowBatchArrays::lower)
        .def_readonly("upper", &RowBatchArrays::upper)
        .def_readonly("names", &RowBatchArrays::names)
        .def("__len__", [](const RowBatchArrays& rows) { return rows.lower.size(); });

    py::class_<ColumnBatchArrays>(
        m,
        "ColumnBatch",
        "Columns to add, in compressed sparse column form (numpy copies): column j has the "
        "entries row_indices[starts[j]:starts[j + 1]] with the matching values.")
        .def(py::init([](InputArray<ColumnId> ids,
                         InputArray<double>
                             cost,
                         InputArray<double>
                             lower,
                         InputArray<double>
                             upper,
                         InputArray<std::uint32_t>
                             starts,
                         InputArray<std::uint32_t>
                             row_indices,
                         InputArray<double>
                             values) {
                 ColumnBatchArrays columns{.ids = std::move(ids),
                                           .cost = std::move(cost),
                                           .lower = std::move(lower),
                                           .upper = std::move(upper),
                                           .starts = std::move(starts),
                                           .row_indices = std::move(row_indices),
                                           .values = std::move(values)};
                 static_cast<void>(columns.view());
                 return columns;
             }),
             py::arg("ids"),
             py::arg("cost"),
             py::arg("lower"),
             py::arg("upper"),
             py::arg("starts"),
             py::arg("row_indices"),
             py::arg("values"))
        .def_readonly("ids", &ColumnBatchArrays::ids)
        .def_readonly("cost", &ColumnBatchArrays::cost)
        .def_readonly("lower", &ColumnBatchArrays::lower)
        .def_readonly("upper", &ColumnBatchArrays::upper)
        .def_readonly("starts", &ColumnBatchArrays::starts)
        .def_readonly("row_indices", &ColumnBatchArrays::row_indices)
        .def_readonly("values", &ColumnBatchArrays::values)
        .def("__len__", [](const ColumnBatchArrays& columns) { return columns.ids.size(); });

    // ── MasterSolver ──────────────────────────────────────────────────────────

    py::class_<MasterSolver, PythonMasterSolver, py::smart_holder>(
        m,
        "MasterSolver",
        "Adapter to the LP/MIP solver holding the master, seen as a minimization with normalized "
        "duals (≥ row: dual ≥ 0, ≤ row: dual ≤ 0). Subclass it to use another solver from "
        "Python: override number_of_rows, row_bounds (returns (lower, upper)), add_columns, "
        "solve_relaxation, objective, duals (returns one value per row), primal (returns one "
        "value per id), solve_integer, set_option and capabilities; add_rows, farkas (returns a "
        "ray or None), dual_tolerance, largest_user_cost and restore_user_model are optional.")
        .def(py::init<>())
        .def("number_of_rows", &MasterSolver::number_of_rows)
        .def("row_bounds",
             [](const MasterSolver& solver) {
                 const size_t rows = solver.number_of_rows();
                 py::array_t<double> lower(static_cast<py::ssize_t>(rows));
                 py::array_t<double> upper(static_cast<py::ssize_t>(rows));
                 solver.row_bounds({lower.mutable_data(), rows}, {upper.mutable_data(), rows});
                 return py::make_tuple(lower, upper);
             })
        .def(
            "add_rows",
            [](MasterSolver& solver, const RowBatchArrays& rows) { solver.add_rows(rows.view()); },
            py::arg("rows"))
        .def(
            "add_columns",
            [](MasterSolver& solver, const ColumnBatchArrays& columns) {
                solver.add_columns(columns.view());
            },
            py::arg("columns"))
        .def("solve_relaxation",
             &MasterSolver::solve_relaxation,
             py::arg("params") = RelaxationParams{},
             py::call_guard<py::gil_scoped_release>())
        .def("objective", &MasterSolver::objective)
        .def("duals",
             [](const MasterSolver& solver) {
                 const size_t rows = solver.number_of_rows();
                 py::array_t<double> duals(static_cast<py::ssize_t>(rows));
                 solver.duals({duals.mutable_data(), rows});
                 return duals;
             })
        .def(
            "primal",
            [](const MasterSolver& solver, const InputArray<ColumnId>& ids) {
                const auto id_view = to_span(ids, "MasterSolver.primal");
                py::array_t<double> values(static_cast<py::ssize_t>(id_view.size()));
                solver.primal(id_view, {values.mutable_data(), id_view.size()});
                return values;
            },
            py::arg("ids"))
        .def("solve_integer",
             &MasterSolver::solve_integer,
             py::arg("params") = IntegerParams{},
             py::call_guard<py::gil_scoped_release>())
        .def("farkas",
             [](const MasterSolver& solver) -> py::object {
                 const size_t rows = solver.number_of_rows();
                 py::array_t<double> ray(static_cast<py::ssize_t>(rows));
                 if (!solver.farkas({ray.mutable_data(), rows})) {
                     return py::none();
                 }
                 return ray;
             })
        .def(
            "set_option",
            [](MasterSolver& solver, const std::string& name, const OptionValue& value) {
                solver.set_option(name, value);
            },
            py::arg("name"),
            py::arg("value"))
        .def("capabilities", &MasterSolver::capabilities)
        .def("dual_tolerance", &MasterSolver::dual_tolerance)
        .def("largest_user_cost", &MasterSolver::largest_user_cost)
        .def("restore_user_model", &MasterSolver::restore_user_model);

    // ── Pricer ────────────────────────────────────────────────────────────────

    const PricingRequest request_defaults;
    py::class_<PricingRequest>(m, "PricingRequest", "What the loop asks of one pricing call.")
        .def(py::init([](double reduced_cost_threshold, double timeout_s) {
                 return PricingRequest{.reduced_cost_threshold = reduced_cost_threshold,
                                       .timeout_s = timeout_s};
             }),
             py::kw_only(),
             py::arg("reduced_cost_threshold") = request_defaults.reduced_cost_threshold,
             py::arg("timeout_s") = request_defaults.timeout_s)
        .def_readwrite("reduced_cost_threshold",
                       &PricingRequest::reduced_cost_threshold,
                       "Return only columns whose reduced cost (without the convexity dual) is "
                       "below this value.")
        .def_readwrite("timeout_s", &PricingRequest::timeout_s);

    const PricingResult result_defaults;
    py::class_<PricingResult>(m, "PricingResult", "What one pricing call returns.")
        .def(py::init([](std::vector<Solution> columns,
                         PricingStatus status,
                         double min_reduced_cost_bound,
                         bool exact_stage) {
                 return PricingResult{.columns = std::move(columns),
                                      .status = status,
                                      .min_reduced_cost_bound = min_reduced_cost_bound,
                                      .exact_stage = exact_stage};
             }),
             py::kw_only(),
             py::arg("columns") = std::vector<Solution>{},
             py::arg("status") = result_defaults.status,
             py::arg("min_reduced_cost_bound") = result_defaults.min_reduced_cost_bound,
             py::arg("exact_stage") = result_defaults.exact_stage)
        .def_readwrite("columns",
                       &PricingResult::columns,
                       "The new columns (a copy: assign a new list to change them); each "
                       "Solution.column holds its cost and rows.")
        .def_readwrite("status", &PricingResult::status)
        .def_readwrite("min_reduced_cost_bound", &PricingResult::min_reduced_cost_bound)
        .def_readwrite("exact_stage",
                       &PricingResult::exact_stage,
                       "Only an exact call that is Optimal or Infeasible, or a call answering "
                       "NotApplicable, proves that no improving column exists.");

    const PricingChanges changes_defaults;
    py::class_<PricingChanges>(m, "PricingChanges", "What changed in the pricing problem.")
        .def(py::init([](bool costs, bool bounds, bool structure) {
                 return PricingChanges{.costs = costs, .bounds = bounds, .structure = structure};
             }),
             py::kw_only(),
             py::arg("costs") = changes_defaults.costs,
             py::arg("bounds") = changes_defaults.bounds,
             py::arg("structure") = changes_defaults.structure)
        .def_readwrite("costs", &PricingChanges::costs)
        .def_readwrite("bounds", &PricingChanges::bounds)
        .def_readwrite("structure", &PricingChanges::structure);

    py::class_<Pricer, PythonPricer, py::smart_holder>(
        m,
        "Pricer",
        "Finds columns with a negative reduced cost for one block of the master. Subclass it to "
        "price in Python: override price(duals, request), returning a PricingResult; update and "
        "cost_scale are optional.")
        .def(py::init<>())
        .def(
            "price",
            [](Pricer& pricer, const InputArray<double>& duals, const PricingRequest& request) {
                const auto dual_view = to_span(duals, "Pricer.price");
                return ActiveCall::run_interruptible(
                    [&] { return pricer.price(dual_view, request); });
            },
            py::arg("duals"),
            py::arg("request") = PricingRequest{})
        .def("update", &Pricer::update, py::arg("changes"))
        .def("cost_scale", &Pricer::cost_scale);

    py::class_<AnyGraphPricer, Pricer, py::smart_holder>(
        m,
        "GraphPricer",
        "A pricer over a bound ResourceGraph, run in C++: each call writes the reduced costs into "
        "the graph, then runs the stages from the first until one finds a path below the "
        "threshold or proves that none exists. Built by rcspp.column_generation.GraphPricer.")
        .def(py::init([](const py::handle& graph,
                         const py::list& stages,
                         size_t cost_index,
                         bool preprocess) {
                 const auto found = graph_pricer_factories().find(Py_TYPE(graph.ptr()));  // NOLINT
                 if (found == graph_pricer_factories().end()) {
                     throw py::type_error(
                         "GraphPricer: expected a bound resource graph with a real resource");
                 }
                 return std::make_unique<AnyGraphPricer>(
                     found->second(graph, stages, cost_index, preprocess));
             }),
             py::arg("graph"),
             py::arg("stages"),
             py::arg("cost_index") = 0,
             py::arg("preprocess") = true,
             py::keep_alive<1, 2>());

    // ── Tolerances and initializations ────────────────────────────────────────

    py::class_<ToleranceContext>(m, "ToleranceContext", "What a tolerance may depend on.")
        .def(py::init([](double cost_scale) { return ToleranceContext{.cost_scale = cost_scale}; }),
             py::kw_only(),
             py::arg("cost_scale") = ToleranceContext{}.cost_scale)
        .def_readwrite("cost_scale", &ToleranceContext::cost_scale);

    py::class_<Tolerances, PythonTolerances, py::smart_holder>(
        m,
        "Tolerances",
        "The tolerances of a run. Subclass it to compute them in Python: override "
        "reduced_cost, feasibility and optimality_gap, each taking a ToleranceContext.")
        .def(py::init<>())
        .def("reduced_cost", &Tolerances::reduced_cost, py::arg("context"))
        .def("feasibility", &Tolerances::feasibility, py::arg("context"))
        .def("optimality_gap", &Tolerances::optimality_gap, py::arg("context"));

    py::class_<AbsoluteTolerances, Tolerances, py::smart_holder>(
        m,
        "AbsoluteTolerances",
        "Fixed tolerances, whatever the scale of the costs (the default).")
        .def(py::init<double, double, double>(),
             py::arg("reduced_cost") = default_reduced_cost_tolerance,
             py::arg("feasibility") = default_feasibility_tolerance,
             py::arg("optimality_gap") = default_optimality_gap);

    py::class_<RelativeTolerances, Tolerances, py::smart_holder>(
        m,
        "RelativeTolerances",
        "Reduced-cost and gap tolerances multiplied by the cost scale; the feasibility tolerance "
        "is not scaled.")
        .def(py::init<double, double, double>(),
             py::arg("reduced_cost") = default_reduced_cost_tolerance,
             py::arg("feasibility") = default_feasibility_tolerance,
             py::arg("optimality_gap") = default_optimality_gap);

    // ── MasterProblem ─────────────────────────────────────────────────────────

    py::class_<RowRange>(m, "RowRange", "A block of consecutive rows: first … first + count − 1.")
        .def_readonly("first", &RowRange::first)
        .def_readonly("count", &RowRange::count);

    const ConvexityRow convexity_defaults;
    py::class_<ConvexityRow>(
        m,
        "ConvexityRow",
        "The convexity row of a pricer: a row of the user's model (existing_row), or one the "
        "master creates, Σ x ≤ max_paths. Set at most one; neither: no convexity row.")
        .def(py::init([](std::optional<size_t> existing_row, double max_paths) {
                 return ConvexityRow{.existing_row = existing_row, .max_paths = max_paths};
             }),
             py::kw_only(),
             py::arg("existing_row") = convexity_defaults.existing_row,
             py::arg("max_paths") = convexity_defaults.max_paths)
        .def_readwrite("existing_row", &ConvexityRow::existing_row)
        .def_readwrite("max_paths", &ConvexityRow::max_paths);

    py::class_<MasterColumn>(m, "MasterColumn", "A column of the master.")
        .def_readonly("id", &MasterColumn::id)
        .def_readonly("pricer",
                      &MasterColumn::pricer,
                      "Index of its pricer; MasterProblem.no_pricer for an artificial column.")
        .def_readonly("tag", &MasterColumn::tag)
        .def_readonly("solution", &MasterColumn::solution);

    py::class_<MasterProblem, py::smart_holder> master(
        m,
        "MasterProblem",
        "The restricted master problem: rows, pricers and the columns generated so far. Keeps "
        "its master solver and pricers alive.");
    master.attr("no_pricer") = MasterProblem::no_pricer;
    master.def(py::init<MasterSolver&>(), py::arg("solver"), py::keep_alive<1, 2>())
        .def("add_rows",
             py::overload_cast<size_t, RowSense, double>(&MasterProblem::add_rows),
             py::arg("count"),
             py::arg("sense"),
             py::arg("right_hand_side"))
        .def(
            "add_rows",
            [](MasterProblem& problem,
               const InputArray<double>& lower,
               const InputArray<double>& upper,
               const std::vector<std::string>& names) {
                return problem.add_rows(to_span(lower, "MasterProblem.add_rows"),
                                        to_span(upper, "MasterProblem.add_rows"),
                                        names);
            },
            py::arg("lower"),
            py::arg("upper"),
            py::arg("names") = std::vector<std::string>{})
        .def("add_pricer",
             &MasterProblem::add_pricer,
             py::arg("pricer"),
             py::arg("convexity") = ConvexityRow{},
             py::keep_alive<1, 2>())
        .def("add_columns", &MasterProblem::add_columns, py::arg("pricer"), py::arg("columns"))
        .def(
            "add_artificial_columns",
            [](MasterProblem& problem,
               const std::vector<size_t>& rows,
               const InputArray<double>& coefficients,
               double cost) {
                problem.add_artificial_columns(
                    rows,
                    to_span(coefficients, "MasterProblem.add_artificial_columns"),
                    cost);
            },
            py::arg("rows"),
            py::arg("coefficients"),
            py::arg("cost"))
        .def("number_of_rows", &MasterProblem::number_of_rows)
        .def("number_of_columns", &MasterProblem::number_of_columns)
        .def("number_of_pricers", &MasterProblem::number_of_pricers)
        .def("pricer",
             &MasterProblem::pricer,
             py::arg("index"),
             py::return_value_policy::reference_internal)
        .def("convexity_row", &MasterProblem::convexity_row, py::arg("index"))
        .def(
            "columns",
            [](const MasterProblem& problem) {
                const auto columns = problem.columns();
                return std::vector<MasterColumn>(columns.begin(), columns.end());
            },
            "Every column, in id order (copies).")
        .def("artificial_column_ids",
             [](const MasterProblem& problem) { return to_array(problem.artificial_column_ids()); })
        .def("artificial_cost", &MasterProblem::artificial_cost)
        .def("initialized", &MasterProblem::initialized)
        .def("mark_initialized", &MasterProblem::mark_initialized)
        .def("solver", &MasterProblem::solver, py::return_value_policy::reference_internal);

    py::class_<MasterInitialization, PythonMasterInitialization, py::smart_holder>(
        m,
        "MasterInitialization",
        "Makes the first master LP solvable. Subclass it to do so in Python: override "
        "initialize(master, context), called once before the first LP solve.")
        .def(py::init<>())
        .def("initialize",
             &MasterInitialization::initialize,
             py::arg("master"),
             py::arg("context"));

    py::class_<ArtificialColumns, MasterInitialization, py::smart_holder> artificial(
        m,
        "ArtificialColumns",
        "Single-phase big-M: one costly artificial column per row that x = 0 violates (the "
        "default). A cost of 0 means automatic: automatic_cost_factor times the cost scale.");
    artificial.attr("automatic_cost_factor") = ArtificialColumns::automatic_cost_factor;
    artificial.def(py::init<double>(), py::arg("cost") = 0.0)
        .def("cost", &ArtificialColumns::cost, py::arg("context"));

    py::class_<NoInitialization, MasterInitialization, py::smart_holder>(
        m,
        "NoInitialization",
        "Leaves the master as it is: for a model already feasible.")
        .def(py::init<>());

    // ── ColumnGeneration ──────────────────────────────────────────────────────

    const StatisticsParams statistics_defaults;
    py::class_<StatisticsParams>(m, "StatisticsParams", "Which statistics a run collects.")
        .def(py::init([](bool timers) { return StatisticsParams{.timers = timers}; }),
             py::kw_only(),
             py::arg("timers") = statistics_defaults.timers)
        .def_readwrite("timers", &StatisticsParams::timers);

    py::class_<Statistics>(m, "Statistics", "Counters and times of one run.")
        .def_readonly("generated_columns", &Statistics::generated_columns)
        .def_readonly("artificial_columns", &Statistics::artificial_columns)
        .def_readonly("artificial_cost", &Statistics::artificial_cost)
        .def_readonly("total_seconds", &Statistics::total_seconds)
        .def_readonly("relaxation_seconds", &Statistics::relaxation_seconds)
        .def_readonly("pricing_seconds", &Statistics::pricing_seconds);

    py::class_<IterationInfo>(m, "IterationInfo", "What the iteration hook sees (a copy).")
        .def_readonly("iteration", &IterationInfo::iteration)
        .def_readonly("objective", &IterationInfo::objective)
        .def_readonly("new_columns", &IterationInfo::new_columns)
        .def_readonly("total_columns", &IterationInfo::total_columns)
        .def_readonly("elapsed_seconds", &IterationInfo::elapsed_seconds);

    py::class_<ConvergenceContext>(
        m,
        "ConvergenceContext",
        "What the convergence hook sees and may change; valid only during the call.")
        .def_property_readonly(
            "master",
            [](const ConvergenceContext& context) -> MasterProblem& { return context.master; },
            py::return_value_policy::reference)
        .def_property_readonly("iteration",
                               [](const ConvergenceContext& context) { return context.iteration; })
        .def_property_readonly("objective",
                               [](const ConvergenceContext& context) { return context.objective; })
        .def_property_readonly(
            "duals",
            [](const ConvergenceContext& context) { return to_array(context.duals); })
        .def_property_readonly(
            "values",
            [](const ConvergenceContext& context) { return to_array(context.values); },
            "Value of every column, indexed like MasterProblem.columns().");

    const ColumnGenerationParams params_defaults;
    py::class_<ColumnGenerationParams>(m, "ColumnGenerationParams", "Options of a run.")
        .def(py::init([](size_t max_iterations,
                         double timeout_s,
                         RelaxationParams relaxation,
                         std::shared_ptr<Tolerances>
                             tolerances,
                         std::shared_ptr<MasterInitialization>
                             initialization,
                         StatisticsParams statistics,
                         std::function<void(IterationInfo)>
                             on_iteration,
                         std::function<ConvergenceAction(ConvergenceContext&)>
                             on_converged) {
                 ColumnGenerationParams params{.max_iterations = max_iterations,
                                               .timeout_s = timeout_s,
                                               .relaxation = relaxation,
                                               .tolerances = std::move(tolerances),
                                               .initialization = std::move(initialization),
                                               .statistics = statistics,
                                               .on_converged = std::move(on_converged)};
                 set_iteration_hook(params, std::move(on_iteration));
                 return params;
             }),
             py::kw_only(),
             py::arg("max_iterations") = params_defaults.max_iterations,
             py::arg("timeout_s") = params_defaults.timeout_s,
             py::arg("relaxation") = params_defaults.relaxation,
             py::arg("tolerances") = py::none(),
             py::arg("initialization") = py::none(),
             py::arg("statistics") = params_defaults.statistics,
             py::arg("on_iteration") = py::none(),
             py::arg("on_converged") = py::none())
        .def_readwrite("max_iterations", &ColumnGenerationParams::max_iterations)
        .def_readwrite("timeout_s", &ColumnGenerationParams::timeout_s)
        .def_readwrite("relaxation",
                       &ColumnGenerationParams::relaxation,
                       "Options of every LP solve; its time limit is capped by what is left of "
                       "timeout_s.")
        .def_property(
            "tolerances",
            [](const ColumnGenerationParams& params) {
                return std::const_pointer_cast<Tolerances>(params.tolerances);
            },
            [](ColumnGenerationParams& params, std::shared_ptr<Tolerances> tolerances) {
                params.tolerances = std::move(tolerances);
            },
            "None: AbsoluteTolerances with its defaults.")
        .def_readwrite("initialization",
                       &ColumnGenerationParams::initialization,
                       "None: ArtificialColumns.")
        .def_readwrite("statistics", &ColumnGenerationParams::statistics)
        .def_property(
            "on_iteration",
            [](const ColumnGenerationParams& params) { return params.on_iteration; },
            &set_iteration_hook,
            "Called after every iteration with an IterationInfo; read-only.")
        .def_readwrite("on_converged",
                       &ColumnGenerationParams::on_converged,
                       "Called after each proven convergence with a ConvergenceContext; returns "
                       "a ConvergenceAction.");

    py::class_<ColumnGenerationResult>(m, "ColumnGenerationResult", "Result of a run.")
        .def_readonly("status", &ColumnGenerationResult::status)
        .def_readonly("objective", &ColumnGenerationResult::objective)
        .def_readonly("lower_bound", &ColumnGenerationResult::lower_bound)
        .def_readonly("iterations", &ColumnGenerationResult::iterations)
        .def_readonly("statistics", &ColumnGenerationResult::statistics);

    py::class_<ColumnGeneration, py::smart_holder>(
        m,
        "ColumnGeneration",
        "The column generation loop: solve the master LP, price its duals, add columns. Keeps "
        "its master alive.")
        .def(py::init<MasterProblem&, ColumnGenerationParams>(),
             py::arg("master"),
             py::arg("params") = ColumnGenerationParams{},
             py::keep_alive<1, 2>())
        .def(
            "run",
            [](ColumnGeneration& loop) {
                return ActiveCall::run_interruptible([&loop] { return loop.run(); });
            },
            "Runs until convergence or a limit, then has the master solver restore the user's "
            "model. Python pricers and master solvers are called with the GIL held.")
        .def(
            "solve_restricted_master_integer",
            [](ColumnGeneration& loop, const IntegerParams& params) {
                return ActiveCall::run_interruptible(
                    [&loop, &params] { return loop.solve_restricted_master_integer(params); });
            },
            py::arg("params") = IntegerParams{})
        .def("params", &ColumnGeneration::params, py::return_value_policy::reference_internal)
        .def("tolerance_context", &ColumnGeneration::tolerance_context);

    // ── Master solver self-test ───────────────────────────────────────────────

    m.attr("default_self_test_tolerance") = default_self_test_tolerance;

    py::class_<MasterSolverSelfTestReport>(m,
                                           "MasterSolverSelfTestReport",
                                           "Outcome of master_solver_self_test.")
        .def_readonly("failures",
                      &MasterSolverSelfTestReport::failures,
                      "One line per failed check: the model, the step, what was checked, the "
                      "expected value and the value the master solver gave.")
        .def("passed", &MasterSolverSelfTestReport::passed);

    m.def("master_solver_self_test",
          &master_solver_self_test,
          py::arg("minimizing"),
          py::arg("maximizing"),
          py::arg("tolerance") = default_self_test_tolerance,
          py::call_guard<py::gil_scoped_release>(),
          "Checks that a master solver follows the conventions of MasterSolver, on an LP whose "
          "answers are known. minimizing and maximizing are two master solvers of the same kind "
          "over empty models, one minimizing and one maximizing, able to add rows; both must give "
          "the same answers. Returns every failed check; raises ValueError when a master solver "
          "already has rows or cannot add rows.");
}
