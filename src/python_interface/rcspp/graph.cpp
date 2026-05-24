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
        .value("Greedy", SolverAlgorithm::Greedy);

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
        .def_readwrite("tabu_tenure", &PyAlgorithmParams::tabu_tenure)
        .def_readwrite("forbidden_tabu", &PyAlgorithmParams::forbidden_tabu)
        .def_readwrite("tabu_random_noise", &PyAlgorithmParams::tabu_random_noise)
        .def_readwrite("seed", &PyAlgorithmParams::seed);

    py::class_<PyBucketAlgorithmParams, PyAlgorithmParams>(m, "BucketAlgorithmParams")
        .def(py::init<>())
        .def_readwrite("range_buckets", &PyBucketAlgorithmParams::range_buckets)
        .def_readwrite("bucket_resource_index", &PyBucketAlgorithmParams::bucket_resource_index)
        .def_readwrite("sort_resource_index", &PyBucketAlgorithmParams::sort_resource_index)
        .def_readwrite("bucket_resource_type", &PyBucketAlgorithmParams::bucket_resource_type);

    py::class_<Column>(m, "Column")
        .def(py::init<>())
        .def_readwrite("cost", &Column::cost)
        .def_readwrite("rows", &Column::rows);

    py::class_<Solution>(m, "Solution")
        .def(py::init<>())
        .def_readwrite("cost", &Solution::cost)
        .def_readwrite("path_node_ids", &Solution::path_node_ids)
        .def_readwrite("path_arc_ids", &Solution::path_arc_ids)
        .def_readwrite("column", &Solution::column);

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
                 py::arg("dual_rows") = std::vector<Row>{},
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
        .def_readwrite("dual_rows", &Arc<RealRC>::dual_rows)
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
