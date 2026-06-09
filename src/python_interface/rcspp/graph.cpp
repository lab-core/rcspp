// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// graph_impl.hpp defines PYBIND11_USE_SMART_HOLDER_AS_DEFAULT before the pybind11 includes.
#include <csignal>

#include "graph_impl.hpp"

// ─── SIGINT state ─────────────────────────────────────────────────────────────
// Declared extern in graph_impl.hpp; defined here (one TU owns the storage).
std::atomic<bool> g_py_interrupted{false};
std::atomic<int> g_active_calls{0};

#ifndef _WIN32
static struct sigaction g_old_sigint_sa = {};
#else
// POSIX sigaction is unavailable on Windows; store the handler returned by signal().
static void (*g_old_sigint_handler)(int) = SIG_DFL;
#endif

static void py_sigint_handler(int sig) {
    ActiveCall::mark_interrupted();
    // Forward to Python's handler only when no C++ call is active; otherwise
    // the binding raises KeyboardInterrupt itself after re-acquiring the GIL.
    if (!ActiveCall::any_active()) {
#ifndef _WIN32
        auto* h = g_old_sigint_sa.sa_handler;
        if (h != nullptr && h != SIG_DFL && h != SIG_IGN) {
            h(sig);
        }
#else
        if (g_old_sigint_handler != SIG_DFL && g_old_sigint_handler != SIG_IGN &&
            g_old_sigint_handler != SIG_ERR) {
            g_old_sigint_handler(sig);
        }
#endif
    }
}

// Called from Python between long-running steps (e.g. CG iterations) to raise
// KeyboardInterrupt if a SIGINT was received since the last solve() call.
// Also processes any pending Python signals via PyErr_CheckSignals.
void py_check_interrupted() {
    ActiveCall::check_if_throw_error();
    if (PyErr_CheckSignals() != 0) {
        throw py::error_already_set();
    }
}

// Called once from PYBIND11_MODULE (main thread) to install the handler.
void init_sigint_handler() {
#ifndef _WIN32
    struct sigaction new_sa = {};  // NOLINT
    new_sa.sa_handler = py_sigint_handler;
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = 0;  // no SA_RESTART — let the signal interrupt blocking calls
    sigaction(SIGINT, &new_sa, &g_old_sigint_sa);
#else
    g_old_sigint_handler = signal(SIGINT, py_sigint_handler);
#endif
}

// ─── Concrete type aliases ────────────────────────────────────────────────────

using RealRC = ResourceTypeComposition<RealResource>;
using RealGraph = Graph<RealRC>;
using RealRG = ResourceGraph<RealResource>;

// ─── init_graph ───────────────────────────────────────────────────────────────

void init_graph(py::module_& m) {
    m.def("check_interrupted",
          &py_check_interrupted,
          "Raise KeyboardInterrupt if a SIGINT was received since the last solve.");

    // ── Algorithm enum ────────────────────────────────────────────────────────

    py::enum_<SolverAlgorithm>(m, "Algorithm")
        .value("Simple", SolverAlgorithm::Simple)
        .value("Pushing", SolverAlgorithm::Pushing)
        .value("Pulling", SolverAlgorithm::Pulling)
        .value("Greedy", SolverAlgorithm::Greedy)
        .value("Tabu", SolverAlgorithm::Tabu)
        .value("AStar", SolverAlgorithm::AStar);

    // ── AlgorithmStatus enum ──────────────────────────────────────────────────

    py::enum_<AlgorithmStatus>(m, "AlgorithmStatus")
        .value("Complete", AlgorithmStatus::COMPLETE)
        .value("Timeout", AlgorithmStatus::TIMEOUT)
        .value("MaxSolutions", AlgorithmStatus::MAX_SOLUTIONS)
        .value("MaxPhases", AlgorithmStatus::MAX_PHASES)
        .value("Interrupted", AlgorithmStatus::INTERRUPTED)
        .value("MemoryLimit", AlgorithmStatus::MEMORY_LIMIT);

    // ── SolveResult ───────────────────────────────────────────────────────────

    py::class_<SolveResult>(m, "SolveResult")
        .def(py::init<>())
        .def_readwrite("solutions", &SolveResult::solutions)
        .def_readwrite("status", &SolveResult::status)
        .def("status_string", &SolveResult::status_string)
        // Sequence protocol — lets existing code treat SolveResult like list[Solution].
        .def("__len__", [](const SolveResult& r) { return r.solutions.size(); })
        .def(
            "__iter__",
            [](const SolveResult& r) {
                return py::make_iterator(r.solutions.begin(), r.solutions.end());
            },
            py::keep_alive<0, 1>())
        .def(
            "__getitem__",
            [](const SolveResult& r, py::ssize_t i) -> const Solution& {
                if (i < 0) {
                    i += static_cast<py::ssize_t>(r.solutions.size());
                }
                if (i < 0 || static_cast<size_t>(i) >= r.solutions.size()) {
                    throw py::index_error("index out of range");
                }
                return r.solutions[static_cast<size_t>(i)];
            },
            py::return_value_policy::reference_internal)
        .def("__bool__", [](const SolveResult& r) { return !r.solutions.empty(); })
        .def("__repr__", [](const SolveResult& r) {
            return "SolveResult(status=" + r.status_string() +
                   ", solutions=" + std::to_string(r.solutions.size()) + ")";
        });

    // ── Shared scalar types ───────────────────────────────────────────────────

    py::class_<Row>(m, "Row")
        .def(py::init<>())
        .def(py::init([](size_t index, long double coefficient) {
                 return Row{.index = index, .coefficient = coefficient};
             }),
             py::arg("index"),
             py::arg("coefficient"))
        .def_readwrite("index", &Row::index)
        .def_readwrite("coefficient", &Row::coefficient);

    py::class_<PyAlgorithmParams>(m, "AlgorithmParams")
        .def(py::init<>())
        .def("check", &PyAlgorithmParams::check)
        .def("could_be_non_optimal", &PyAlgorithmParams::could_be_non_optimal)
        .def_readwrite("stop_after_X_solutions", &PyAlgorithmParams::stop_after_X_solutions)
        .def_readwrite("return_dominated_solutions", &PyAlgorithmParams::return_dominated_solutions)
        .def_readwrite("use_pool", &PyAlgorithmParams::use_pool)
        .def_readwrite("num_labels_to_extend_by_node",
                       &PyAlgorithmParams::num_labels_to_extend_by_node)
        .def_readwrite("num_max_phases", &PyAlgorithmParams::num_max_phases)
        .def_readwrite("max_iterations", &PyAlgorithmParams::max_iterations)
        .def_readwrite("timeout_s",
                       &PyAlgorithmParams::timeout_s,
                       "Wall-clock timeout in seconds; solve() returns early when elapsed >= "
                       "timeout_s (default: inf).")
        .def_readwrite("tolerance",
                       &PyAlgorithmParams::tolerance,
                       "Numerical tolerance for cost comparisons (default 1e-9).")
        .def_readwrite(
            "release_after_solve",
            &PyAlgorithmParams::release_after_solve,
            "If true (default), release label memory after solve(). Set to false when the "
            "same algorithm is called repeatedly in a tight loop to avoid shrink_to_fit() "
            "overhead.")
        .def_readwrite("tabu_tenure", &PyAlgorithmParams::tabu_tenure)
        .def_readwrite("forbidden_tabu", &PyAlgorithmParams::forbidden_tabu)
        .def_readwrite("tabu_random_noise", &PyAlgorithmParams::tabu_random_noise)
        .def_readwrite("seed", &PyAlgorithmParams::seed)
        // ── Memory-limit parameters ──────────────────────────────────────
        .def_readwrite("max_memory_gb",
                       &PyAlgorithmParams::max_memory_gb,
                       "Hard cap on process RSS in GiB (0 = unlimited). "
                       "The solver stops early and returns whatever solutions have been found.")
        .def_readwrite("limit_to_available_ram",
                       &PyAlgorithmParams::limit_to_available_ram,
                       "Derive limit from currently-available system RAM.")
        .def_readwrite("limit_to_total_ram",
                       &PyAlgorithmParams::limit_to_total_ram,
                       "Derive limit from total physical RAM.")
        .def_readwrite("memory_limit_fraction",
                       &PyAlgorithmParams::memory_limit_fraction,
                       "Fraction of RAM to use as limit (default 0.9).")
        .def_readwrite("memory_check_interval",
                       &PyAlgorithmParams::memory_check_interval,
                       "Main-loop iterations between RSS checks (default 50 000).")
        .def_readwrite("memory_pressure_fraction",
                       &PyAlgorithmParams::memory_pressure_fraction,
                       "RSS/limit fraction that triggers queue pruning (default 0.8).")
        .def_readwrite("memory_pressure_max_labels_per_node",
                       &PyAlgorithmParams::memory_pressure_max_labels_per_node,
                       "Max labels per node when under memory pressure (default 200).");

    py::class_<PyBucketAlgorithmParams, PyAlgorithmParams>(m, "BucketAlgorithmParams")
        .def(py::init<>())
        .def_readwrite("range_buckets", &PyBucketAlgorithmParams::range_buckets)
        .def_readwrite("bucket_resource_index", &PyBucketAlgorithmParams::bucket_resource_index)
        .def_readwrite("sort_resource_index", &PyBucketAlgorithmParams::sort_resource_index)
        .def_readwrite("bucket_resource_type", &PyBucketAlgorithmParams::bucket_resource_type);

    py::class_<Column>(m, "Column")
        .def(py::init<>())
        .def_readwrite("cost", &Column::cost)
        .def_readwrite("rows", &Column::rows)
        // to_arrays(): extract the LP cost and row (index, coefficient) data as numpy
        // arrays in one C++ call. Lets the Python pricing pool bulk-read a column without
        // the per-Row pybind attribute overhead that dominates add()/update().
        .def(
            "to_arrays",
            [](const Column& col) -> py::tuple {
                const size_t nr = col.rows.size();
                auto idx = py::array_t<int64_t>(static_cast<py::ssize_t>(nr));
                auto coef = py::array_t<double>(static_cast<py::ssize_t>(nr));
                auto ip = idx.mutable_unchecked<1>();
                auto cp = coef.mutable_unchecked<1>();
                for (size_t i = 0; i < nr; ++i) {
                    ip(static_cast<py::ssize_t>(i)) = static_cast<int64_t>(col.rows[i].index);
                    cp(static_cast<py::ssize_t>(i)) = static_cast<double>(col.rows[i].coefficient);
                }
                return py::make_tuple(col.cost, idx, coef);
            },
            "Return (cost, row_indices, row_coefficients) as numpy arrays "
            "(int64 indices, float64 coefficients) — avoids per-Row Python overhead.");

    py::class_<Solution>(m, "Solution")
        .def(py::init<>())
        .def_readwrite("cost", &Solution::cost)
        .def_readwrite("path_node_ids", &Solution::path_node_ids)
        // path_arc_ids is exposed via a property whose setter recomputes the content hash, so a
        // Solution built the idiomatic Python way (default ctor + attribute assignment) gets the
        // same hash as one built via the value constructor. A plain def_readwrite would leave
        // hash_ stale (the empty-path hash), collapsing SolutionPool's hash_index_ into one bucket.
        .def_property(
            "path_arc_ids",
            [](const Solution& s) -> const std::vector<size_t>& { return s.path_arc_ids; },
            [](Solution& s, std::vector<size_t> v) {
                s.path_arc_ids = std::move(v);
                s.rehash();
            })
        .def_readwrite("column", &Solution::column)
        .def("get_hash", &Solution::get_hash)
        .def(
            "to_arrays",
            [](const Solution& sol) -> py::tuple {
                // nodes: int64 array
                const auto& nids = sol.path_node_ids;
                auto nodes = py::array_t<int64_t>(static_cast<py::ssize_t>(nids.size()));
                auto nptr = nodes.mutable_unchecked<1>();
                py::ssize_t k = 0;
                for (size_t n : nids) {
                    nptr(k++) = static_cast<int64_t>(n);
                }
                // rows: separate int64 (index) and float64 (coefficient) arrays
                const auto& rows = sol.column.rows;
                size_t nr = rows.size();
                auto ridx = py::array_t<int64_t>(static_cast<py::ssize_t>(nr));
                auto rcoeff = py::array_t<double>(static_cast<py::ssize_t>(nr));
                auto rip = ridx.mutable_unchecked<1>();
                auto rcp = rcoeff.mutable_unchecked<1>();
                for (size_t i = 0; i < nr; ++i) {
                    rip(static_cast<py::ssize_t>(i)) = static_cast<int64_t>(rows[i].index);
                    rcp(static_cast<py::ssize_t>(i)) = static_cast<double>(rows[i].coefficient);
                }
                return py::make_tuple(sol.cost, nodes, ridx, rcoeff);
            },
            "Return (cost, node_ids, row_indices, row_coefficients) as numpy arrays. "
            "Avoids per-Row Python object overhead during column extraction.");

    // ══════════════════════════════════════════════════════════════════════════
    // RealResource — primary bindings with full Node/Arc/Graph public exposure
    // ══════════════════════════════════════════════════════════════════════════

    {
        py::class_<RealGraph> g(m, "Graph");
        bind_graph_methods(g);
        g.def(py::init<>())
            .def("add_node",
                 &RealGraph::add_node,
                 py::arg("id"),
                 py::arg("source") = false,
                 py::arg("sink") = false,
                 py::return_value_policy::reference)
            .def("add_arc",
                 py::overload_cast<size_t, size_t, double, std::vector<Row>>(&RealGraph::add_arc),
                 py::arg("origin_id"),
                 py::arg("destination_id"),
                 py::arg("cost") = 0.0,
                 py::arg("rows") = std::vector<Row>{},
                 py::return_value_policy::reference);
    }

    py::class_<Node<RealRC>>(m, "Node")
        .def(py::init<size_t, bool, bool>())
        .def_readonly("id", &Node<RealRC>::id)
        .def("pos", &Node<RealRC>::pos)
        .def_readonly("source", &Node<RealRC>::source)
        .def_readonly("sink", &Node<RealRC>::sink)
        .def_readwrite("in_arcs", &Node<RealRC>::in_arcs)
        .def_readwrite("out_arcs", &Node<RealRC>::out_arcs)
        .def_readwrite("resource", &Node<RealRC>::resource)
        .def("__str__", &Node<RealRC>::to_string)
        .def("__repr__", &Node<RealRC>::to_string);

    py::class_<Arc<RealRC>>(m, "Arc")
        .def_readonly("id", &Arc<RealRC>::id)
        .def(
            "origin",
            [](const Arc<RealRC>& a) -> Node<RealRC>* { return a.origin; },
            py::return_value_policy::reference)
        .def(
            "destination",
            [](const Arc<RealRC>& a) -> Node<RealRC>* { return a.destination; },
            py::return_value_policy::reference)
        .def_readwrite("extender", &Arc<RealRC>::extender)
        .def_readwrite("cost", &Arc<RealRC>::cost)
        .def_readwrite("rows", &Arc<RealRC>::rows)
        .def("__str__", &Arc<RealRC>::to_string)
        .def("__repr__", &Arc<RealRC>::to_string);

    // ── Real resource graph — explicit block to avoid re-registering Node/Arc/Graph
    {
        py::class_<RealRG, RealGraph> rg(m, "_real_resource_graph");
        bind_rg_methods<RealRG, RealRC, RealResource, RealResource>(rg);
        rg.def(py::init<>());
        bind_resource_graph_impl<RealRG, RealRC, RealResource>(rg);
    }

    // ══════════════════════════════════════════════════════════════════════════
    // Mixed-resource graphs — split across graph_mix2.cpp and graph_mix3.cpp
    // for faster parallel compilation.
    // ══════════════════════════════════════════════════════════════════════════
    init_graph_mix2(m);
    init_graph_mix3(m);
}
