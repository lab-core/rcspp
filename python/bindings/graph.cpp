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
        .value("AStar", SolverAlgorithm::AStar)
        .value("Bidirectional", SolverAlgorithm::Bidirectional);

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
        .def_readonly("bounded_by_half_way",
                      &SolveResult::bounded_by_half_way,
                      "Whether the bidirectional half-way bound was in force. False from every "
                      "other algorithm. A bidirectional solve whose clock fails validation is "
                      "slower than a forward one, not merely un-accelerated, so this is worth "
                      "checking rather than assuming.")
        .def_readonly("number_of_joined_paths",
                      &SolveResult::number_of_joined_paths,
                      "How many distinct complete paths only the bidirectional join pass produced. "
                      "0 from every other algorithm. Zero with a correct answer means the answer "
                      "came from a search reaching a terminal, not from the join.")
        .def_readonly("memory_pressure_triggered",
                      &SolveResult::memory_pressure_triggered,
                      "Whether memory pressure trimmed this solve, in which case the result may "
                      "not be optimal. Reported by every algorithm. status stays 'complete' when "
                      "it happens -- the label sets really were exhausted, of what survived the "
                      "trim -- so code that treats 'complete' as a proof of optimality has to "
                      "check this too.")
        .def_readonly("forward_labels",
                      &SolveResult::forward_labels,
                      "Labels surviving dominance in the forward containers when the solve "
                      "ended. Reported by the four exact forward algorithms and by "
                      "bidirectional; 0 from the heuristics, which do not use these containers.")
        .def_readonly("backward_labels",
                      &SolveResult::backward_labels,
                      "Labels surviving dominance in the backward containers. Non-zero only "
                      "from a search that ran backwards, so forward_labels / backward_labels is "
                      "meaningful only when both are non-zero -- an imbalance far from 1, "
                      "iteration after iteration, is a half-way point in the wrong place.")
        .def_readonly("dominance_checks",
                      &SolveResult::dominance_checks,
                      "Dominance comparisons performed across every label container, both "
                      "directions. Divided by the surviving label count it is the cost of the "
                      "dominance rule per label kept.")
        .def_readonly("join_pairs_tested",
                      &SolveResult::join_pairs_tested,
                      "Pairs the join asked the merge rule about; 0 when it did not run.")
        .def_readonly("join_truncated",
                      &SolveResult::join_truncated,
                      "Whether the join stopped at max_join_pairs with a pair left to test. A "
                      "'complete' result with this set is not a proof.")
        .def_readonly("half_way_point_used",
                      &SolveResult::half_way_point_used,
                      "The half-way point H this solve actually used; 0.0 when the bound was "
                      "off. Not the same as the requested params.half_way_point: the bound "
                      "disables itself when the critical resource fails validation. Check "
                      "bounded_by_half_way to tell 'the bound was off' from 'H really was 0'.")
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
            std::string text = "SolveResult(status=" + r.status_string() +
                               ", solutions=" + std::to_string(r.solutions.size());
            if (r.bounded_by_half_way || r.number_of_joined_paths > 0) {
                text += ", bounded_by_half_way=" +
                        std::string(r.bounded_by_half_way ? "True" : "False") +
                        ", joined=" + std::to_string(r.number_of_joined_paths);
            }
            // Shown only when set, so it stands out.
            if (r.memory_pressure_triggered) {
                text += ", memory_pressure_triggered=True";
            }
            return text + ")";
        });

    // ── HalfWayController ─────────────────────────────────────────────────────
    // The Python route to a half-way point that adapts between solves. `dynamic_half_way` is not
    // bound, because `rg.solve(...)` builds a fresh algorithm every call and the flag's state would
    // die with it. The controller is a plain value object instead: the caller keeps it across the
    // loop, reads `h` into `params.half_way_point` before each solve, and hands it the result
    // afterwards. That keeps the damping state RouteOpt's rule needs without giving the bindings a
    // stateful handle on a solver, which they have never had.

    py::enum_<HalfWayMove>(m, "HalfWayMove")
        .value("Unchanged", HalfWayMove::Unchanged)
        .value("Skipped", HalfWayMove::Skipped)
        .value("Frozen", HalfWayMove::Frozen)
        .value("Down", HalfWayMove::Down)
        .value("Up", HalfWayMove::Up)
        .value("TowardCentre", HalfWayMove::TowardCentre);

    py::class_<HalfWayControllerParams>(m, "HalfWayControllerParams")
        .def(py::init<>())
        .def_readwrite("dead_zone",
                       &HalfWayControllerParams::dead_zone,
                       "Relative imbalance |f - b| / min(f, b) tolerated before H moves "
                       "(default 0.2).")
        .def_readwrite("initial_step",
                       &HalfWayControllerParams::initial_step,
                       "First multiplicative step, H <- H * (1 -/+ step) (default 0.2).")
        .def_readwrite("step_decay",
                       &HalfWayControllerParams::step_decay,
                       "The step is divided by this whenever the direction reverses "
                       "(default 2.0).")
        .def_readwrite("min_step",
                       &HalfWayControllerParams::min_step,
                       "Floor under the decaying step (default 0.025).")
        .def_readwrite("min_fraction",
                       &HalfWayControllerParams::min_fraction,
                       "H is kept at or above min_fraction * R, with R = 2 * initial H "
                       "(default 0.05).")
        .def_readwrite("max_fraction",
                       &HalfWayControllerParams::max_fraction,
                       "H is kept at or below max_fraction * R (default 0.95).");

    py::class_<HalfWayController>(m, "HalfWayController")
        .def(py::init<double, HalfWayControllerParams>(),
             py::arg("initial_h"),
             py::arg("params") = HalfWayControllerParams{},
             "Seed the controller at initial_h, which must be positive: it is the first "
             "half_way_point, and it fixes the range R = 2 * initial_h that H is kept inside. "
             "Raises ValueError for a non-positive initial_h or out-of-range params.")
        .def(
            "update",
            [](HalfWayController& controller, const SolveResult& result, bool truncated) {
                return controller.update(half_way_observation(result, truncated));
            },
            py::arg("result"),
            py::arg("truncated") = false,
            "Fold one bidirectional solve's result into H and return the move made. Learns only "
            "from a complete solve with the bound in force; pass truncated=True if you capped "
            "the search with num_labels_to_extend_by_node, which the status cannot show.")
        .def("reset",
             &HalfWayController::reset,
             "Return H and the step to their initial values and forget the direction.")
        .def_property_readonly("h",
                               &HalfWayController::h,
                               "The half_way_point the next solve should use.")
        .def_property_readonly("initial_h", &HalfWayController::initial_h)
        .def_property_readonly("range", &HalfWayController::range, "R = 2 * initial_h.")
        .def_property_readonly("step", &HalfWayController::step)
        .def_property_readonly("last_move", &HalfWayController::last_move)
        .def_property_readonly("observations", &HalfWayController::observations)
        .def_property_readonly("moves", &HalfWayController::moves)
        .def_property("frozen",
                      &HalfWayController::frozen,
                      &HalfWayController::set_frozen,
                      "While True, update() records nothing and H stays put.")
        .def("__repr__", [](const HalfWayController& controller) {
            return "HalfWayController(h=" + std::to_string(controller.h()) +
                   ", step=" + std::to_string(controller.step()) +
                   ", last_move=" + to_string(controller.last_move()) +
                   (controller.frozen() ? ", frozen=True" : "") + ")";
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
                       "Max labels per node when under memory pressure (default 200).")
        // -- Bidirectional parameters ------------------------------------
        .def_readwrite("critical_resource_index",
                       &PyAlgorithmParams::critical_resource_index,
                       "Index, within the cost resource type's slot, of the resource used as the "
                       "bidirectional clock. It must be monotone (never decreasing along an arc) "
                       "and extend backwards as a threshold -- a time window or a budget, not a "
                       "plain additive resource and never the cost. When it is neither, the "
                       "half-way bound switches itself off: the solve stays correct, just slower. "
                       "Ignored by every other algorithm (default 0).")
        .def_readwrite("half_way_point",
                       &PyAlgorithmParams::half_way_point,
                       "Value H at which each direction's search stops on the critical resource: "
                       "forward discards labels above H, backward discards labels below it, and "
                       "the join pairs what is left. The resource's range is taken as [0, 2H], so "
                       "set H to about half the clock's range. 0 (the default) TURNS THE BOUND "
                       "OFF -- it does not derive one: both searches then run to completion and "
                       "the join considers every pair, which is correct but slower than a forward "
                       "solve rather than faster. Check result.bounded_by_half_way to see which "
                       "you got.")
        .def_readwrite("join_column_budget",
                       &PyAlgorithmParams::join_column_budget,
                       "Bidirectional only: the most paths the join returns, the cheapest kept. "
                       "Never stops the search, so a 'complete' status still means exhaustive, and "
                       "the optimum is always kept. Default: no budget.")
        .def_readwrite("max_join_pairs",
                       &PyAlgorithmParams::max_join_pairs,
                       "Bidirectional only: the most pairs the join may test before it stops. When "
                       "it binds, result.join_truncated is set and the result is not a proof. "
                       "Default: no cap.")
        .def_readwrite("join_after_early_stop",
                       &PyAlgorithmParams::join_after_early_stop,
                       "Bidirectional only: whether the join still runs after a timeout or the "
                       "memory limit (default True). An interrupt always skips it.");
    // dynamic_half_way is deliberately NOT bound. It adapts H between solves on one persistent
    // algorithm object, and rg.solve(...) builds a fresh algorithm every call -- so here the flag
    // could never take effect, and would only invite someone to set it and conclude the policy is
    // broken. The Python route is HalfWayController, bound above next to SolveResult.

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
