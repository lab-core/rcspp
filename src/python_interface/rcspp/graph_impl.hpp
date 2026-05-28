// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Must be defined before any pybind11 header is included.
#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <atomic>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>

#include "rcspp/rcspp.hpp"
#include "resource_types.hpp"

namespace py = pybind11;

using namespace rcspp;

// ─── SIGINT globals (defined in graph.cpp) ────────────────────────────────────
// g_py_interrupted: set by the C signal handler; polled by the labelling loop.
// g_active_calls:   counts long-running C++ bindings holding the release guard.
extern std::atomic<bool> g_py_interrupted;
extern std::atomic<int> g_active_calls;

// ─── run_interruptible ────────────────────────────────────────────────────────
// Releases the GIL, calls f(), re-acquires the GIL, then raises KeyboardInterrupt
// if g_py_interrupted was set during the call.  Handles both void and non-void callables.
class ActiveCall {
    private:
        struct ActiveCallGuard {
                ActiveCallGuard() { g_active_calls.fetch_add(1, std::memory_order_relaxed); }
                ~ActiveCallGuard() { g_active_calls.fetch_sub(1, std::memory_order_relaxed); }
                ActiveCallGuard(const ActiveCallGuard&) = delete;
                ActiveCallGuard& operator=(const ActiveCallGuard&) = delete;
        };

    public:
        static bool any_active() { return g_active_calls.load(std::memory_order_relaxed) > 0; }

        static bool is_interrupted() { return g_py_interrupted.load(std::memory_order_relaxed); }

        template <typename F>
        static auto run_interruptible(F&& f) {
            using R = std::invoke_result_t<F>;
            if constexpr (std::is_void_v<R>) {
                {
                    ActiveCallGuard guard;
                    py::gil_scoped_release release;
                    std::forward<F>(f)();
                }
                check_if_throw_error();
            } else {
                std::optional<R> result;
                {
                    ActiveCallGuard guard;
                    py::gil_scoped_release release;
                    result.emplace(std::forward<F>(f)());
                }
                check_if_throw_error();
                return std::move(*result);
            }
        }

        static void mark_interrupted() { g_py_interrupted.store(true, std::memory_order_relaxed); }

        static void check_if_throw_error() {
            if (g_py_interrupted.exchange(false, std::memory_order_relaxed)) {
                PyErr_SetNone(PyExc_KeyboardInterrupt);
                throw py::error_already_set();
            }
        }
};

// ─── Python-facing non-template AlgorithmParams ──────────────────────────────

struct PyAlgorithmParams : AlgorithmBaseParams {
        template <typename LC>
        [[nodiscard]] AlgorithmParams<LC> to_params(LC label_container = LC{}) const {
            AlgorithmParams<LC> p(*this, std::move(label_container));
            return p;
        }
};

struct PyBucketAlgorithmParams : PyAlgorithmParams {
        size_t range_buckets = 100;  // NOLINT(readability-magic-numbers)
        size_t bucket_resource_index = 0;
        size_t sort_resource_index = 0;
        std::string bucket_resource_type;  // empty = use CostRC; "real", "int", etc. for explicit
};

// ─── Algorithm dispatch table ─────────────────────────────────────────────────

enum class SolverAlgorithm { Simple, Pushing, Pulling, Greedy, Tabu, AStar };

template <SolverAlgorithm E, template <typename, typename> class Algo>
struct AlgoEntry {
        static constexpr SolverAlgorithm value = E;
        template <typename RG, typename CostRC, typename LC>
        static SolveResult run(RG& rg, double ub, AlgorithmParams<LC> p, bool pre, size_t ci) {
            return rg.template solve<Algo, CostRC, LC>(ub, std::move(p), pre, ci);
        }
};

// Dispatch entry for A*: injects cost_index into params and binds CostRC.
template <SolverAlgorithm E>
struct AStarAlgoEntry {
        static constexpr SolverAlgorithm value = E;
        template <typename RG, typename CostRC, typename LC>
        static SolveResult run(RG& rg, double ub, AlgorithmParams<LC> p, bool pre, size_t ci) {
            p.heuristic_cost_index = ci;
            return rg.template solve<AStarAlgoBound<CostRC>::template Algo, CostRC, LC>(
                ub,
                std::move(p),
                pre,
                ci);
        }
};

using AlgorithmTable = std::tuple<AlgoEntry<SolverAlgorithm::Simple, SimpleDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Pushing, PushingDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Pulling, PullingDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Greedy, GreedyAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Tabu, TabuSearchAlgorithm>,
                                  AStarAlgoEntry<SolverAlgorithm::AStar>>;

template <typename RG, typename CostRC, typename LC, typename... Entries>
SolveResult dispatch_algorithm_impl(SolverAlgorithm alg, RG& rg, double ub, AlgorithmParams<LC> p,
                                    bool pre, size_t ci, std::tuple<Entries...>* /*tag*/) {
    SolveResult result;
    [[maybe_unused]] bool matched =
        ((Entries::value == alg
              ? (result = Entries::template run<RG, CostRC, LC>(rg, ub, p, pre, ci), true)
              : false) ||
         ...);
    return result;
}

template <typename RG, typename CostRC, typename LC>
SolveResult dispatch_algorithm(SolverAlgorithm alg, RG& rg, double ub, AlgorithmParams<LC> p,
                               bool pre, size_t ci) {
    p.should_stop = &ActiveCall::is_interrupted;
    return dispatch_algorithm_impl<RG, CostRC, LC>(alg,
                                                   rg,
                                                   ub,
                                                   p,
                                                   pre,
                                                   ci,
                                                   static_cast<AlgorithmTable*>(nullptr));
}

// ─── Resource type → pybind11 method name ────────────────────────────────────

template <typename T>
constexpr const char* add_resource_method_name();

#define GEN_ADD_RESOURCE_NAME(name, scalar, RT)                   \
    template <>                                                   \
    inline constexpr const char* add_resource_method_name<RT>() { \
        return "add_" #name "_resource";                          \
    }
RCSPP_ALL_RESOURCES(GEN_ADD_RESOURCE_NAME)
#undef GEN_ADD_RESOURCE_NAME

// ─── Python type name per resource type ──────────────────────────────────────

template <typename T>
constexpr const char* py_type_name();

#define GEN_PY_TYPE_NAME(name, scalar, RT)            \
    template <>                                       \
    inline constexpr const char* py_type_name<RT>() { \
        return #name;                                 \
    }
RCSPP_ALL_RESOURCES(GEN_PY_TYPE_NAME)
#undef GEN_PY_TYPE_NAME

// ─── Resource type resolver ───────────────────────────────────────────────────
// Iterates over ResourceTypes..., finds the first numerical type whose py_type_name
// matches type_name, and calls callback.operator()<RT>() with it.
// Throws py::value_error(param_name) if no match is found.

template <typename... ResourceTypes, typename Callback>
void with_resource_type(const std::string& type_name, const char* param_name, Callback&& cb) {
    bool matched = false;
    if constexpr (sizeof...(ResourceTypes) > 0) {
        auto try_type = [&]<typename RT>() -> bool {
            if constexpr (!is_numerical_resource_v<RT>) {
                return false;
            }
            if (type_name != py_type_name<RT>()) {
                return false;
            }
            cb.template operator()<RT>();
            return true;
        };
        matched = (try_type.template operator()<ResourceTypes>() || ...);
    }
    if (!matched) {
        throw py::value_error(std::string("Unknown or non-numerical ") + param_name + ": '" +
                              type_name + "'");
    }
}

// ─── Bucket solve dispatcher ──────────────────────────────────────────────────
// Selects LabelBuckets<RT, RT, RC> based on py_p.bucket_resource_type.
// Empty string → use CostRC (default). Non-numerical types are skipped.

template <typename RG, typename RC, typename CostRC, typename... ResourceTypes>
SolveResult run_bucket_solve(SolverAlgorithm alg, RG& rg, double ub,
                             const PyBucketAlgorithmParams& py_p, bool pre, size_t ci) {
    auto check_index = [&](const char* param, size_t idx, size_t count) {
        if (idx >= count) {
            throw py::value_error(std::string(param) + " " + std::to_string(idx) +
                                  " out of range (graph has " + std::to_string(count) +
                                  " resource(s) of the required type, valid range [0, " +
                                  std::to_string(count - 1) + "])");
        }
    };

    SolveResult result;
    auto run_func = [&]<typename RT>() {
        const auto& factory = rg.get_resource_factory();
        check_index("bucket_resource_index",
                    py_p.bucket_resource_index,
                    factory.template get_num_resource_type<RT>());
        check_index("sort_resource_index",
                    py_p.sort_resource_index,
                    factory.template get_num_resource_type<CostRC>());
        using BucketLC = LabelBuckets<RT, CostRC, RC>;
        BucketLC lc(py_p.range_buckets, py_p.bucket_resource_index, py_p.sort_resource_index);
        auto p = py_p.template to_params<BucketLC>(std::move(lc));
        result = dispatch_algorithm<RG, CostRC, BucketLC>(alg, rg, ub, std::move(p), pre, ci);
    };

    // Run with default template value CostRC
    if (py_p.bucket_resource_type.empty()) {
        run_func.template operator()<CostRC>();
    } else {
        // Otherwise, retrieve the right template
        with_resource_type<ResourceTypes...>(py_p.bucket_resource_type,
                                             "bucket_resource_type",
                                             run_func);
    }
    return result;
}

// ─── CostRC auto-selection ────────────────────────────────────────────────────
// Picks the first numerical resource in the pack; falls back to RealResource sentinel.

template <typename... RTs>
struct SelectCostRC {
        using type = RealResource;
};

template <typename RT, typename... RTs>
struct SelectCostRC<RT, RTs...> {
        using type = std::conditional_t<is_numerical_resource_v<RT>, RT,
                                        typename SelectCostRC<RTs...>::type>;
};

// ─── Helper: bind common Graph base methods ───────────────────────────────────

template <typename G>
py::class_<G>& bind_graph_methods(py::class_<G>& c) {
    using ArcType = std::remove_pointer_t<decltype(std::declval<const G&>().get_arc(0))>;

    return c.def("get_node", &G::get_node, py::arg("id"), py::return_value_policy::reference)
        .def("get_arc", &G::get_arc, py::arg("id"), py::return_value_policy::reference)
        .def("get_arcs",
             &G::get_arcs,
             py::arg("origin_id"),
             py::arg("destination_id"),
             py::return_value_policy::reference)
        .def("node_ids", &G::get_node_ids)
        .def("arc_ids",
             [](const G& g) {
                 std::vector<size_t> ids;
                 ids.reserve(g.get_number_of_arcs());
                 g.for_each_arc([&](const auto& arc) { ids.push_back(arc.id); });
                 return ids;
             })
        .def("source_node_ids", &G::get_source_node_ids)
        .def("sink_node_ids", &G::get_sink_node_ids)
        .def("number_of_nodes", &G::get_number_of_nodes)
        .def("number_of_arcs", &G::get_number_of_arcs)
        .def("is_source", &G::is_source, py::arg("node_id"))
        .def("is_sink", &G::is_sink, py::arg("node_id"))
        .def("to_string", &G::to_string, py::arg("print_arcs") = false)
        .def("__str__", [](const G& g) { return g.to_string(); })
        .def("__repr__", [](const G& g) { return g.to_string(); })
        .def("sort_nodes", [](G& g) { g.sort_nodes(); })
        .def(
            "sort_nodes",
            [](G& g, py::function comp) {
                g.sort_nodes([comp](const auto* n1, const auto* n2) -> bool {
                    return py::cast<bool>(comp(n1, n2));
                });
            },
            py::arg("comp"))
        .def("reserve",
             &G::reserve,
             py::arg("n_nodes"),
             py::arg("n_arcs"),
             "Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.")
        .def("remove_arc", static_cast<bool (G::*)(size_t)>(&G::remove_arc), py::arg("arc_id"))
        .def(
            "remove_arc",
            [](G& g, ArcType* arc) { return g.remove_arc(*arc); },
            py::arg("arc"))
        .def("restore_arc", static_cast<bool (G::*)(size_t)>(&G::restore_arc), py::arg("arc_id"))
        .def(
            "restore_arc",
            [](G& g, ArcType* arc) { return g.restore_arc(*arc); },
            py::arg("arc"))
        .def("removed_arc_ids", &G::get_removed_arc_ids)
        .def("get_removed_arc",
             &G::get_removed_arc,
             py::arg("arc_id"),
             py::return_value_policy::reference)
        .def(
            "remove_arcs_if",
            [](G& g, py::function pred) {
                return g.remove_arcs_if(
                    [&pred](const ArcType& arc) { return py::cast<bool>(pred(&arc)); });
            },
            py::arg("pred"))
        .def(
            "restore_arcs_if",
            [](G& g, py::function pred) {
                return g.restore_arcs_if(
                    [&pred](const ArcType& arc) { return py::cast<bool>(pred(&arc)); });
            },
            py::arg("pred"))
        .def("remove_arcs",
             static_cast<std::vector<size_t> (G::*)(const std::vector<size_t>&)>(&G::remove_arcs),
             py::arg("arc_ids"),
             py::call_guard<py::gil_scoped_release>())
        .def("restore_arcs",
             static_cast<std::vector<size_t> (G::*)(const std::vector<size_t>&)>(&G::restore_arcs),
             py::arg("arc_ids"),
             py::call_guard<py::gil_scoped_release>())
        .def("force_arc",
             static_cast<std::vector<size_t> (G::*)(size_t)>(&G::force_arc),
             py::arg("arc_id"),
             "Remove all other out-arcs from the arc's origin and all other in-arcs to its "
             "destination. Returns the ids of the removed arcs.")
        .def(
            "force_arc",
            [](G& g, ArcType* arc) { return g.force_arc(*arc); },
            py::arg("arc"),
            "Remove all other out-arcs from the arc's origin and all other in-arcs to its "
            "destination. Returns the ids of the removed arcs.")
        .def("add_rows_to_arc",
             &G::add_rows_to_arc,
             py::arg("arc_id"),
             py::arg("rows"),
             "Append rows to an arc's rows. Returns false if arc_id is invalid.")
        .def("next_arc_id",
             &G::next_arc_id,
             "Return the next arc ID that will be assigned by add_arc().")
        .def(
            "_add_rows_bulk",
            [](G& g, py::array_t<double, py::array::c_style | py::array::forcecast> rows) {
                // rows must be sorted by arc_id (column 0) — the Python side
                // guarantees this via np.argsort in _build_base_graph.
                //
                // Process contiguous runs of the same arc_id: look up the arc
                // once per run, reserve capacity once, then push_back every row
                // in the run directly into the arc's rows vector.  This avoids:
                //   • one std::vector<Row> heap allocation per row (old code),
                //   • repeated bounds checks and pointer dereferences per row.
                auto r = rows.unchecked<2>();
                const auto n = r.shape(0);
                py::ssize_t i = 0;
                while (i < n) {
                    const auto arc_id = static_cast<size_t>(r(i, 0));
                    auto* arc = g.get_arc(arc_id);
                    // Find the end of this arc's run.
                    py::ssize_t j = i + 1;
                    while (j < n && static_cast<size_t>(r(j, 0)) == arc_id) {
                        ++j;
                    }
                    if (arc != nullptr) {
                        auto& dr = arc->rows;
                        dr.reserve(dr.size() + static_cast<size_t>(j - i));
                        for (py::ssize_t k = i; k < j; ++k) {
                            dr.push_back(Row{
                                .index = static_cast<size_t>(r(k, 1)),
                                .coefficient = static_cast<long double>(r(k, 2)),
                            });
                        }
                    }
                    i = j;
                }
            },
            py::arg("rows"),
            py::call_guard<py::gil_scoped_release>(),
            "Bulk-append rows from a (N, 3) float64 array [arc_id, row_index, coeff]. "
            "The array must be sorted by arc_id (column 0).");
}

// ─── Helper: bind common ResourceGraph methods ────────────────────────────────
// ResourceTypes: the resource types in the graph (used to dispatch LabelBuckets).

template <typename RG, typename RC, typename CostRC = RealResource, typename... ResourceTypes>
py::class_<RG, Graph<RC>>& bind_rg_methods(py::class_<RG, Graph<RC>>& c) {
    using N = Node<RC>;
    constexpr double INF = std::numeric_limits<double>::infinity();

    return c
        .def("add_node",
             static_cast<N& (RG::*)(size_t, bool, bool)>(&RG::add_node),
             py::arg("id"),
             py::arg("source") = false,
             py::arg("sink") = false,
             py::return_value_policy::reference)
        .def("get_resource_factory", &RG::get_resource_factory, py::return_value_policy::reference)
        .def(
            "solve",
            [](RG& rg,
               SolverAlgorithm alg,
               double ub,
               const PyBucketAlgorithmParams& py_p,
               bool pre,
               size_t ci) -> SolveResult {
                return ActiveCall::run_interruptible([&] {
                    return run_bucket_solve<RG, RC, CostRC, ResourceTypes...>(alg,
                                                                              rg,
                                                                              ub,
                                                                              py_p,
                                                                              pre,
                                                                              ci);
                });
            },
            py::arg("algorithm") = SolverAlgorithm::Simple,
            py::arg("upper_bound") = INF,
            py::arg("params"),
            py::arg("preprocess") = true,
            py::arg("cost_index") = 0)
        .def(
            "solve",
            [](RG& rg,
               SolverAlgorithm alg,
               double ub,
               const PyAlgorithmParams& py_p,
               bool pre,
               size_t ci) -> SolveResult {
                using LC = LabelList<RC>;
                auto p = py_p.template to_params<LC>();
                return ActiveCall::run_interruptible(
                    [&] { return dispatch_algorithm<RG, CostRC, LC>(alg, rg, ub, p, pre, ci); });
            },
            py::arg("algorithm") = SolverAlgorithm::Simple,
            py::arg("upper_bound") = INF,
            py::arg("params") = PyAlgorithmParams{},
            py::arg("preprocess") = true,
            py::arg("cost_index") = 0)
        .def("preprocess_feasibility", &RG::process_feasibility)
        .def("is_connected",
             &RG::is_connected,
             py::arg("origin_node_id"),
             py::arg("destination_node_id"))
        .def(
            "_add_nodes_bulk",
            [](RG& rg, const std::vector<std::tuple<size_t, bool, bool>>& nodes) {
                for (const auto& [id, source, sink] : nodes) {
                    rg.add_node(id, source, sink);
                }
            },
            py::arg("nodes"),
            py::call_guard<py::gil_scoped_release>())
        .def(
            "clone",
            [](RG& rg, bool include_rows, bool clone_removed_arcs) {
                return rg.clone(include_rows, clone_removed_arcs);
            },
            py::arg("include_rows") = true,
            py::arg("clone_removed_arcs") = false,
            py::call_guard<py::gil_scoped_release>(),
            "Clone this ResourceGraph. Arc IDs are stable across the clone. "
            "The clone has an independent remove/restore state.");
}

// ─── Helper: bind one add_resource method ────────────────────────────────────

template <typename RG, typename RC, typename ResourceType>
void bind_add_resource(py::class_<RG, Graph<RC>>& rg) {
    rg.def(add_resource_method_name<ResourceType>(),
           static_cast<void (RG::*)(std::unique_ptr<ExtensionFunction<ResourceType>>,
                                    std::unique_ptr<FeasibilityFunction<ResourceType>>,
                                    std::unique_ptr<CostFunction<ResourceType>>,
                                    std::unique_ptr<DominanceFunction<ResourceType>>)>(
               &RG::template add_resource<ResourceType>),
           py::arg("extension_function"),
           py::arg("feasibility_function"),
           py::arg("cost_function"),
           py::arg("dominance_function"));
}

// ─── Helper: bind resource-specific methods ───────────────────────────────────

template <typename RG, typename RC, typename... ResourceTypes>
void bind_resource_graph_impl(py::class_<RG, Graph<RC>>& rg) {
    using AddArcTuple = std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>;

    (bind_add_resource<RG, RC, ResourceTypes>(rg), ...);

    rg.def(
        "add_arc",
        static_cast<Arc<RC>& (RG::*)(const AddArcTuple&, size_t, size_t, double, std::vector<Row>)>(
            &RG::add_arc),
        py::arg("resource_consumption"),
        py::arg("origin_node_id"),
        py::arg("destination_node_id"),
        py::arg("cost") = 0.0,
        py::arg("rows") = std::vector<Row>{},
        py::return_value_policy::reference);

    rg.def("update_arc",
           static_cast<void (RG::*)(Arc<RC>*, const AddArcTuple&, std::optional<double>)>(
               &RG::update_arc),
           py::arg("arc"),
           py::arg("resource_consumption"),
           py::arg("cost") = std::nullopt);

    rg.def(
        "_add_arcs_bulk",
        [](RG& rg,
           const std::vector<AddArcTuple>& consumptions,
           const std::vector<size_t>& origins,
           const std::vector<size_t>& dests,
           const std::vector<double>& costs,
           const std::vector<std::vector<Row>>& rows) {
            for (size_t i = 0; i < consumptions.size(); ++i) {
                rg.add_arc(consumptions[i], origins[i], dests[i], costs[i], rows[i]);
            }
        },
        py::arg("consumptions"),
        py::arg("origin_ids"),
        py::arg("destination_ids"),
        py::arg("costs"),
        py::arg("rows"),
        py::call_guard<py::gil_scoped_release>());

    if constexpr ((std::is_same_v<ResourceTypes, RealResource> || ...)) {
        rg.def(
            "update_reduced_costs",
            [](RG& rg,
               py::array_t<double, py::array::c_style | py::array::forcecast>
                   duals_arr,
               size_t cost_index) {
                // Buffer access while GIL is held — just a pointer read (O(1)).
                // Copy via fast memcpy into a vector, then release the GIL for
                // the actual reduced-cost computation across all arcs.
                auto buf = duals_arr.request();
                std::vector<double> duals_vec(static_cast<const double*>(buf.ptr),
                                              static_cast<const double*>(buf.ptr) + buf.size);
                ActiveCall::run_interruptible(
                    [&] { rg.template update_reduced_costs<RealResource>(duals_vec, cost_index); });
            },
            py::arg("duals"),
            py::arg("cost_index") = 0);
    }
}

// ─── Helper: build the full block for a set of resource types ─────────────────

template <typename RG, typename RC, typename CostRC, typename... ResourceTypes>
void bind_resource_graph_block(py::module_& m, const char* rg_name, const char* graph_name,
                               const char* node_name, const char* arc_name) {
    py::class_<Node<RC>>(m, node_name)
        .def_readonly("id", &Node<RC>::id)
        .def_readonly("source", &Node<RC>::source)
        .def_readonly("sink", &Node<RC>::sink)
        .def("__str__", &Node<RC>::to_string)
        .def("__repr__", &Node<RC>::to_string);

    py::class_<Arc<RC>>(m, arc_name)
        .def_readonly("id", &Arc<RC>::id)
        .def(
            "origin",
            [](const Arc<RC>& a) -> Node<RC>* { return a.origin; },
            py::return_value_policy::reference)
        .def(
            "destination",
            [](const Arc<RC>& a) -> Node<RC>* { return a.destination; },
            py::return_value_policy::reference)
        .def_readwrite("cost", &Arc<RC>::cost)
        .def_readwrite("rows", &Arc<RC>::rows)
        .def("__str__", &Arc<RC>::to_string)
        .def("__repr__", &Arc<RC>::to_string);

    {
        py::class_<Graph<RC>> g(m, graph_name);
        bind_graph_methods(g);
    }

    py::class_<RG, Graph<RC>> rg(m, rg_name);
    bind_rg_methods<RG, RC, CostRC, ResourceTypes...>(rg);
    rg.def(py::init<>());
    bind_resource_graph_impl<RG, RC, ResourceTypes...>(rg);
}

// ─── Helper: bind a mixed-resource graph ─────────────────────────────────────

template <typename CostRC, typename... RTs>
void bind_mixed_rg(py::module_& m, const char* name) {
    using RC = ResourceTypeComposition<RTs...>;
    using RG = ResourceGraph<RTs...>;
    std::string rg = std::string("_") + name + "_resource_graph";
    std::string g = std::string("_") + name + "_graph";
    std::string n = std::string("_") + name + "_node";
    std::string a = std::string("_") + name + "_arc";
    bind_resource_graph_block<RG, RC, CostRC, RTs...>(m,
                                                      rg.c_str(),
                                                      g.c_str(),
                                                      n.c_str(),
                                                      a.c_str());
}

// ─── Auto class-name builder and binder ──────────────────────────────────────

template <typename... RTs>
std::string auto_mix_name() {
    std::vector<const char*> names = {py_type_name<RTs>()...};
    std::string s;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            s += '_';
        }
        s += names[i];
    }
    return s;
}

template <typename... RTs>
void bind_auto_rg(py::module_& m) {
    using CostRC = typename SelectCostRC<RTs...>::type;
    bind_mixed_rg<CostRC, RTs...>(m, auto_mix_name<RTs...>().c_str());
}

#define BIND_MIX(...) bind_auto_rg<__VA_ARGS__>(m)

// ─── Macros: bind single-resource graph blocks ───────────────────────────────

// clang-format off
#define BIND_SINGLE_NUMERICAL_RG(name, scalar, RT)                                 \
    bind_resource_graph_block<ResourceGraph<RT>, ResourceComposition<RT>, RT, RT>( \
        m,                                                                         \
        "_" #name "_resource_graph",                                               \
        "_" #name "_graph",                                                        \
        "_" #name "_node",                                                         \
        "_" #name "_arc");

#define BIND_SINGLE_CONTAINER_RG(name, scalar, RT)                                           \
    bind_resource_graph_block<ResourceGraph<RT>, ResourceComposition<RT>, RealResource, RT>( \
        m,                                                                                   \
        "_" #name "_resource_graph",                                                         \
        "_" #name "_graph",                                                                  \
        "_" #name "_node",                                                                   \
        "_" #name "_arc");
// clang-format on

// ─── Forward declarations for sub-init functions ─────────────────────────────
void init_graph_mix2(py::module_& /*m*/);
void init_graph_mix3(py::module_& /*m*/);
