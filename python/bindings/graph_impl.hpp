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

        template <typename Callable>
        static auto run_interruptible(Callable&& f) {
            using ResultType = std::invoke_result_t<Callable>;
            if constexpr (std::is_void_v<ResultType>) {
                {
                    ActiveCallGuard guard;
                    py::gil_scoped_release release;
                    std::forward<Callable>(f)();
                }
                check_if_throw_error();
            } else {
                std::optional<ResultType> result;
                {
                    ActiveCallGuard guard;
                    py::gil_scoped_release release;
                    result.emplace(std::forward<Callable>(f)());
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
        template <typename LabelContainerType>
        [[nodiscard]] AlgorithmParams<LabelContainerType> to_params(
            LabelContainerType label_container = LabelContainerType{}) const {
            AlgorithmParams<LabelContainerType> p(*this, std::move(label_container));
            return p;
        }
};

struct PyBucketAlgorithmParams : PyAlgorithmParams {
        size_t bucket_width = 100;  // NOLINT(readability-magic-numbers)
        size_t bucket_resource_index = 0;
        size_t sort_resource_index = 0;
        std::string
            bucket_resource_type;  // empty = use CostResourceType; "real", "int", etc. for explicit
};

// ─── Algorithm dispatch table ─────────────────────────────────────────────────

// Append new values at the end: the integer values are exposed to Python.
enum class SolverAlgorithm { Simple, Pushing, Pulling, Greedy, Tabu, AStar, Bidirectional };

// Dispatch entry for the algorithms that read the search direction from params. An algorithm that
// does not support the requested direction throws std::invalid_argument (ValueError in Python).
template <SolverAlgorithm SolverId, template <typename, typename> class AlgorithmType>
struct AlgoEntry {
        static constexpr SolverAlgorithm value = SolverId;
        template <typename ResourceGraphType, typename CostResourceType,
                  typename LabelContainerType>
        static SolveResult run(ResourceGraphType& rg, double ub,
                               AlgorithmParams<LabelContainerType> p, bool pre, size_t ci) {
            return rg.template solve<AlgorithmType, CostResourceType, LabelContainerType>(
                ub,
                std::move(p),
                pre,
                ci);
        }
};

// Dispatch entry for A*: injects cost_index into params and binds CostResourceType.
template <SolverAlgorithm SolverId>
struct AStarAlgoEntry {
        static constexpr SolverAlgorithm value = SolverId;
        template <typename ResourceGraphType, typename CostResourceType,
                  typename LabelContainerType>
        static SolveResult run(ResourceGraphType& rg, double ub,
                               AlgorithmParams<LabelContainerType> p, bool pre, size_t ci) {
            p.heuristic_cost_index = ci;
            return rg.template solve<AStarAlgoBound<CostResourceType>::template Algo,
                                     CostResourceType,
                                     LabelContainerType>(ub, std::move(p), pre, ci);
        }
};

// Dispatch entry for `algorithm="bidirectional"`: the simple search with direction Bidirectional,
// the clock's type being the cost's. `critical_resource_index` comes from params untouched. A clock
// whose type differs from the cost's needs the C++ API.
template <SolverAlgorithm SolverId>
struct BidirectionalAlgoEntry {
        static constexpr SolverAlgorithm value = SolverId;
        template <typename ResourceGraphType, typename CostResourceType,
                  typename LabelContainerType>
        static SolveResult run(ResourceGraphType& rg, double ub,
                               AlgorithmParams<LabelContainerType> p, bool pre, size_t ci) {
            p.direction = SearchDirection::Bidirectional;
            return rg.template solve<SimpleDominanceAlgorithm,
                                     CostResourceType,
                                     LabelContainerType,
                                     CostResourceType>(ub, std::move(p), pre, ci);
        }
};

using AlgorithmTable = std::tuple<AlgoEntry<SolverAlgorithm::Simple, SimpleDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Pushing, PushingDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Pulling, PullingDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Greedy, GreedyAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Tabu, TabuSearchAlgorithm>,
                                  AStarAlgoEntry<SolverAlgorithm::AStar>,
                                  BidirectionalAlgoEntry<SolverAlgorithm::Bidirectional>>;

template <typename ResourceGraphType, typename CostResourceType, typename LabelContainerType,
          typename... Entries>
SolveResult dispatch_algorithm_impl(SolverAlgorithm alg, ResourceGraphType& rg, double ub,
                                    AlgorithmParams<LabelContainerType> p, bool pre, size_t ci,
                                    std::tuple<Entries...>* /*tag*/) {
    SolveResult result;
    [[maybe_unused]] bool matched =
        ((Entries::value == alg
              ? (result =
                     Entries::template run<ResourceGraphType, CostResourceType, LabelContainerType>(
                         rg,
                         ub,
                         p,
                         pre,
                         ci),
                 true)
              : false) ||
         ...);
    return result;
}

template <typename ResourceGraphType, typename CostResourceType, typename LabelContainerType>
SolveResult dispatch_algorithm(SolverAlgorithm alg, ResourceGraphType& rg, double ub,
                               AlgorithmParams<LabelContainerType> p, bool pre, size_t ci) {
    p.should_stop = &ActiveCall::is_interrupted;
    return dispatch_algorithm_impl<ResourceGraphType, CostResourceType, LabelContainerType>(
        alg,
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

#define GEN_ADD_RESOURCE_NAME(name, scalar, ResourceType)                   \
    template <>                                                             \
    inline constexpr const char* add_resource_method_name<ResourceType>() { \
        return "add_" #name "_resource";                                    \
    }
RCSPP_ALL_RESOURCES(GEN_ADD_RESOURCE_NAME)
#undef GEN_ADD_RESOURCE_NAME

// ─── Python type name per resource type ──────────────────────────────────────

template <typename T>
constexpr const char* py_type_name();

#define GEN_PY_TYPE_NAME(name, scalar, ResourceType)            \
    template <>                                                 \
    inline constexpr const char* py_type_name<ResourceType>() { \
        return #name;                                           \
    }
RCSPP_ALL_RESOURCES(GEN_PY_TYPE_NAME)
#undef GEN_PY_TYPE_NAME

// ─── Resource type resolver ───────────────────────────────────────────────────
// Iterates over ResourceTypes..., finds the first numerical type whose py_type_name
// matches type_name, and calls callback.operator()<ResourceType>() with it.
// Throws py::value_error(param_name) if no match is found.

template <typename... ResourceTypes, typename Callback>
void with_resource_type(const std::string& type_name, const char* param_name, Callback&& cb) {
    bool matched = false;
    if constexpr (sizeof...(ResourceTypes) > 0) {
        auto try_type = [&]<typename ResourceType>() -> bool {
            if constexpr (!is_numerical_resource_v<ResourceType>) {
                return false;
            }
            if (type_name != py_type_name<ResourceType>()) {
                return false;
            }
            cb.template operator()<ResourceType>();
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
// Selects LabelBuckets<ResourceType, ResourceType, CompositionType> based on
// py_p.bucket_resource_type. Empty string → use CostResourceType (default). Non-numerical types are
// skipped.

template <typename ResourceGraphType, typename CompositionType, typename CostResourceType,
          typename... ResourceTypes>
SolveResult run_bucket_solve(SolverAlgorithm alg, ResourceGraphType& rg, double ub,
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
    auto run_func = [&]<typename ResourceType>() {
        const auto& factory = rg.get_resource_factory();
        check_index("bucket_resource_index",
                    py_p.bucket_resource_index,
                    factory.template get_num_resource_type<ResourceType>());
        check_index("sort_resource_index",
                    py_p.sort_resource_index,
                    factory.template get_num_resource_type<CostResourceType>());
        using BucketLC = LabelBuckets<ResourceType, CostResourceType, CompositionType>;
        BucketLC lc(py_p.bucket_width, py_p.bucket_resource_index, py_p.sort_resource_index);
        auto p = py_p.template to_params<BucketLC>(std::move(lc));
        result = dispatch_algorithm<ResourceGraphType, CostResourceType, BucketLC>(alg,
                                                                                   rg,
                                                                                   ub,
                                                                                   std::move(p),
                                                                                   pre,
                                                                                   ci);
    };

    // Run with default template value CostResourceType
    if (py_p.bucket_resource_type.empty()) {
        run_func.template operator()<CostResourceType>();
    } else {
        // Otherwise, retrieve the right template
        with_resource_type<ResourceTypes...>(py_p.bucket_resource_type,
                                             "bucket_resource_type",
                                             run_func);
    }
    return result;
}

// ─── CostResourceType auto-selection ────────────────────────────────────────────────────
// Picks the first numerical resource in the pack; falls back to RealResource sentinel.

template <typename... ResourceTypes>
struct SelectCostResourceType {
        using type = RealResource;
};

template <typename ResourceType, typename... ResourceTypes>
struct SelectCostResourceType<ResourceType, ResourceTypes...> {
        using type = std::conditional_t<is_numerical_resource_v<ResourceType>, ResourceType,
                                        typename SelectCostResourceType<ResourceTypes...>::type>;
};

// ─── Helper: bind common Graph base methods ───────────────────────────────────

template <typename GraphType>
py::class_<GraphType>& bind_graph_methods(py::class_<GraphType>& c) {
    using ArcType = std::remove_pointer_t<decltype(std::declval<const GraphType&>().get_arc(0))>;

    return c
        .def("get_node",
             &GraphType::get_node,
             py::arg("node_id"),
             py::return_value_policy::reference)
        .def("get_arc", &GraphType::get_arc, py::arg("arc_id"), py::return_value_policy::reference)
        .def("get_arcs",
             &GraphType::get_arcs,
             py::arg("origin_node_id"),
             py::arg("destination_node_id"),
             py::return_value_policy::reference)
        .def("node_ids", &GraphType::get_node_ids)
        .def("arc_ids",
             [](const GraphType& g) {
                 std::vector<size_t> ids;
                 ids.reserve(g.get_number_of_arcs());
                 g.for_each_arc([&](const auto& arc) { ids.push_back(arc.id); });
                 return ids;
             })
        .def("source_node_ids", &GraphType::get_source_node_ids)
        .def("sink_node_ids", &GraphType::get_sink_node_ids)
        .def("number_of_nodes", &GraphType::get_number_of_nodes)
        .def("number_of_arcs", &GraphType::get_number_of_arcs)
        .def("is_source", &GraphType::is_source, py::arg("node_id"))
        .def("is_sink", &GraphType::is_sink, py::arg("node_id"))
        .def("to_string", &GraphType::to_string, py::arg("print_arcs") = false)
        .def("__str__", [](const GraphType& g) { return g.to_string(); })
        .def("__repr__", [](const GraphType& g) { return g.to_string(); })
        .def("sort_nodes", [](GraphType& g) { g.sort_nodes(); })
        .def(
            "sort_nodes",
            [](GraphType& g, py::function comp) {
                g.sort_nodes([comp](const auto* n1, const auto* n2) -> bool {
                    return py::cast<bool>(comp(n1, n2));
                });
            },
            py::arg("comp"))
        .def("reserve",
             &GraphType::reserve,
             py::arg("n_nodes"),
             py::arg("n_arcs"),
             "Pre-allocate hash-map buckets to avoid rehashing during bulk inserts.")
        .def("remove_arc",
             static_cast<bool (GraphType::*)(size_t)>(&GraphType::remove_arc),
             py::arg("arc_id"))
        .def(
            "remove_arc",
            [](GraphType& g, ArcType* arc) { return g.remove_arc(*arc); },
            py::arg("arc"))
        .def("restore_arc",
             static_cast<bool (GraphType::*)(size_t)>(&GraphType::restore_arc),
             py::arg("arc_id"))
        .def(
            "restore_arc",
            [](GraphType& g, ArcType* arc) { return g.restore_arc(*arc); },
            py::arg("arc"))
        .def("removed_arc_ids", &GraphType::get_removed_arc_ids)
        .def("get_removed_arc",
             &GraphType::get_removed_arc,
             py::arg("arc_id"),
             py::return_value_policy::reference)
        .def(
            "remove_arcs_if",
            [](GraphType& g, py::function pred) {
                return g.remove_arcs_if(
                    [&pred](const ArcType& arc) { return py::cast<bool>(pred(&arc)); });
            },
            py::arg("pred"))
        .def(
            "restore_arcs_if",
            [](GraphType& g, py::function pred) {
                return g.restore_arcs_if(
                    [&pred](const ArcType& arc) { return py::cast<bool>(pred(&arc)); });
            },
            py::arg("pred"))
        .def("remove_arcs",
             static_cast<std::vector<size_t> (GraphType::*)(const std::vector<size_t>&)>(
                 &GraphType::remove_arcs),
             py::arg("arc_ids"),
             py::call_guard<py::gil_scoped_release>())
        .def("restore_arcs",
             static_cast<std::vector<size_t> (GraphType::*)(const std::vector<size_t>&)>(
                 &GraphType::restore_arcs),
             py::arg("arc_ids"),
             py::call_guard<py::gil_scoped_release>())
        .def("force_arc",
             static_cast<std::vector<size_t> (GraphType::*)(size_t)>(&GraphType::force_arc),
             py::arg("arc_id"),
             "Remove all other out-arcs from the arc's origin and all other in-arcs to its "
             "destination. Returns the ids of the removed arcs.")
        .def(
            "force_arc",
            [](GraphType& g, ArcType* arc) { return g.force_arc(*arc); },
            py::arg("arc"),
            "Remove all other out-arcs from the arc's origin and all other in-arcs to its "
            "destination. Returns the ids of the removed arcs.")
        .def("add_rows_to_arc",
             &GraphType::add_rows_to_arc,
             py::arg("arc_id"),
             py::arg("rows"),
             "Append rows to an arc's rows. Returns false if arc_id is invalid.")
        .def("next_arc_id",
             &GraphType::next_arc_id,
             "Return the next arc ID that will be assigned by add_arc().")
        .def(
            "_add_rows_bulk",
            // By const reference: a by-value array would be destroyed inside the
            // py::call_guard scope below, i.e. with the GIL released.
            [](GraphType& g,
               const py::array_t<double, py::array::c_style | py::array::forcecast>& rows) {
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

template <typename ResourceGraphType, typename CompositionType,
          typename CostResourceType = RealResource, typename... ResourceTypes>
py::class_<ResourceGraphType, Graph<CompositionType>>& bind_rg_methods(
    py::class_<ResourceGraphType, Graph<CompositionType>>& c) {
    using NodeType = Node<CompositionType>;
    constexpr double INF = std::numeric_limits<double>::infinity();

    return c
        .def("add_node",
             static_cast<NodeType& (ResourceGraphType::*)(size_t, bool, bool)>(
                 &ResourceGraphType::add_node),
             py::arg("node_id"),
             py::arg("source") = false,
             py::arg("sink") = false,
             py::return_value_policy::reference)
        .def(
            "solve",
            [](ResourceGraphType& rg,
               SolverAlgorithm alg,
               double ub,
               const PyBucketAlgorithmParams& py_p,
               bool pre,
               size_t ci) -> SolveResult {
                return ActiveCall::run_interruptible([&] {
                    return run_bucket_solve<ResourceGraphType,
                                            CompositionType,
                                            CostResourceType,
                                            ResourceTypes...>(alg, rg, ub, py_p, pre, ci);
                });
            },
            py::arg("algorithm") = SolverAlgorithm::Simple,
            py::arg("upper_bound") = INF,
            py::arg("params"),
            py::arg("preprocess") = true,
            py::arg("cost_index") = 0)
        .def(
            "solve",
            [](ResourceGraphType& rg,
               SolverAlgorithm alg,
               double ub,
               const PyAlgorithmParams& py_p,
               bool pre,
               size_t ci) -> SolveResult {
                using LabelContainerType = LabelList<CompositionType>;
                auto p = py_p.template to_params<LabelContainerType>();
                return ActiveCall::run_interruptible([&] {
                    return dispatch_algorithm<ResourceGraphType,
                                              CostResourceType,
                                              LabelContainerType>(alg, rg, ub, p, pre, ci);
                });
            },
            py::arg("algorithm") = SolverAlgorithm::Simple,
            py::arg("upper_bound") = INF,
            py::arg("params") = PyAlgorithmParams{},
            py::arg("preprocess") = true,
            py::arg("cost_index") = 0)
        .def(
            "check_model",
            [](const ResourceGraphType& rg, SearchDirection direction) {
                return rg.check_model(direction).problems;
            },
            py::arg("direction") = SearchDirection::Bidirectional,
            "Run, without solving, the model checks that a search in `direction` needs. Returns "
            "one string per problem found; empty when the model is ready for that search.")
        .def("remove_infeasible_arcs", &ResourceGraphType::remove_infeasible_arcs)
        .def("is_connected",
             &ResourceGraphType::is_connected,
             py::arg("origin_node_id"),
             py::arg("destination_node_id"))
        .def(
            "_add_nodes_bulk",
            [](ResourceGraphType& rg, const std::vector<std::tuple<size_t, bool, bool>>& nodes) {
                for (const auto& [id, source, sink] : nodes) {
                    rg.add_node(id, source, sink);
                }
            },
            py::arg("nodes"),
            py::call_guard<py::gil_scoped_release>())
        .def(
            "clone",
            [](ResourceGraphType& rg, bool include_rows, bool clone_removed_arcs) {
                return rg.clone(include_rows, clone_removed_arcs);
            },
            py::arg("include_rows") = true,
            py::arg("clone_removed_arcs") = false,
            py::call_guard<py::gil_scoped_release>(),
            "Clone this ResourceGraph. Arc IDs are stable across the clone. "
            "The clone has an independent remove/restore state.");
}

// ─── Helper: bind one add_resource method ────────────────────────────────────

template <typename ResourceGraphType, typename CompositionType, typename ResourceType>
void bind_add_resource(py::class_<ResourceGraphType, Graph<CompositionType>>& rg) {
    rg.def(
        add_resource_method_name<ResourceType>(),
        static_cast<void (ResourceGraphType::*)(std::unique_ptr<ExtensionFunction<ResourceType>>,
                                                std::unique_ptr<FeasibilityFunction<ResourceType>>,
                                                std::unique_ptr<CostFunction<ResourceType>>,
                                                std::unique_ptr<DominanceFunction<ResourceType>>)>(
            &ResourceGraphType::template add_resource<ResourceType>),
        py::arg("extension_function"),
        py::arg("feasibility_function"),
        py::arg("cost_function"),
        py::arg("dominance_function"));
}

// ─── Helper: bind resource-specific methods ───────────────────────────────────

template <typename ResourceGraphType, typename CompositionType, typename... ResourceTypes>
void bind_resource_graph_impl(py::class_<ResourceGraphType, Graph<CompositionType>>& rg) {
    using AddArcTuple = std::tuple<std::vector<ComponentInitializerTypeTuple_t<ResourceTypes>>...>;

    (bind_add_resource<ResourceGraphType, CompositionType, ResourceTypes>(rg), ...);

    rg.def("add_arc",
           static_cast<Arc<CompositionType>& (
               ResourceGraphType::*)(const AddArcTuple&, size_t, size_t, double, std::vector<Row>)>(
               &ResourceGraphType::add_arc),
           py::arg("resource_consumption"),
           py::arg("origin_node_id"),
           py::arg("destination_node_id"),
           py::arg("cost") = 0.0,
           py::arg("rows") = std::vector<Row>{},
           py::return_value_policy::reference);

    rg.def("update_arc",
           static_cast<void (ResourceGraphType::*)(Arc<CompositionType>*,
                                                   const AddArcTuple&,
                                                   std::optional<double>)>(
               &ResourceGraphType::update_arc),
           py::arg("arc"),
           py::arg("resource_consumption"),
           py::arg("cost") = std::nullopt);

    rg.def(
        "_add_arcs_bulk",
        [](ResourceGraphType& rg,
           const std::vector<AddArcTuple>& consumptions,
           const std::vector<size_t>& origins,
           const std::vector<size_t>& dests,
           const std::vector<double>& costs,
           const std::vector<std::vector<Row>>& rows) {
            // Check every arc before adding any, so a refused arc leaves the graph unchanged
            // instead of holding the arcs before it.
            for (size_t i = 0; i < consumptions.size(); ++i) {
                rg.check_new_arc(origins[i], dests[i]);
            }
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
            [](ResourceGraphType& rg,
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

template <typename ResourceGraphType, typename CompositionType, typename CostResourceType,
          typename... ResourceTypes>
void bind_resource_graph_block(py::module_& m, const char* rg_name, const char* graph_name,
                               const char* node_name, const char* arc_name) {
    py::class_<Node<CompositionType>>(m, node_name)
        .def_readonly("id", &Node<CompositionType>::id)
        .def_readonly("source", &Node<CompositionType>::source)
        .def_readonly("sink", &Node<CompositionType>::sink)
        .def("__str__", &Node<CompositionType>::to_string)
        .def("__repr__", &Node<CompositionType>::to_string);

    py::class_<Arc<CompositionType>>(m, arc_name)
        .def_readonly("id", &Arc<CompositionType>::id)
        .def(
            "origin",
            [](const Arc<CompositionType>& a) -> Node<CompositionType>* { return a.origin; },
            py::return_value_policy::reference)
        .def(
            "destination",
            [](const Arc<CompositionType>& a) -> Node<CompositionType>* { return a.destination; },
            py::return_value_policy::reference)
        .def_readwrite("cost", &Arc<CompositionType>::cost)
        .def_readwrite("rows", &Arc<CompositionType>::rows)
        .def("__str__", &Arc<CompositionType>::to_string)
        .def("__repr__", &Arc<CompositionType>::to_string);

    {
        py::class_<Graph<CompositionType>> g(m, graph_name);
        bind_graph_methods(g);
    }

    py::class_<ResourceGraphType, Graph<CompositionType>> rg(m, rg_name);
    bind_rg_methods<ResourceGraphType, CompositionType, CostResourceType, ResourceTypes...>(rg);
    rg.def(py::init<>());
    bind_resource_graph_impl<ResourceGraphType, CompositionType, ResourceTypes...>(rg);
}

// ─── Helper: bind a mixed-resource graph ─────────────────────────────────────

template <typename CostResourceType, typename... ResourceTypes>
void bind_mixed_rg(py::module_& m, const char* name) {
    using CompositionType = ResourceTypeComposition<ResourceTypes...>;
    using ResourceGraphType = ResourceGraph<ResourceTypes...>;
    std::string rg = std::string("_") + name + "_resource_graph";
    std::string g = std::string("_") + name + "_graph";
    std::string n = std::string("_") + name + "_node";
    std::string a = std::string("_") + name + "_arc";
    bind_resource_graph_block<ResourceGraphType,
                              CompositionType,
                              CostResourceType,
                              ResourceTypes...>(m, rg.c_str(), g.c_str(), n.c_str(), a.c_str());
}

// ─── Auto class-name builder and binder ──────────────────────────────────────

template <typename... ResourceTypes>
std::string auto_mix_name() {
    std::vector<const char*> names = {py_type_name<ResourceTypes>()...};
    std::string s;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            s += '_';
        }
        s += names[i];
    }
    return s;
}

template <typename... ResourceTypes>
void bind_auto_rg(py::module_& m) {
    using CostResourceType = typename SelectCostResourceType<ResourceTypes...>::type;
    bind_mixed_rg<CostResourceType, ResourceTypes...>(m, auto_mix_name<ResourceTypes...>().c_str());
}

#define BIND_MIX(...) bind_auto_rg<__VA_ARGS__>(m)

// ─── Forward declarations for sub-init functions ─────────────────────────────
void init_graph_mix2(py::module_& /*m*/);
void init_graph_mix3(py::module_& /*m*/);
