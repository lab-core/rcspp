// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include "rcspp/graph/graph.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <limits>
#include <memory>
#include <tuple>

#include "rcspp/algorithm/greedy.hpp"
#include "rcspp/algorithm/pulling_dominance_algorithm.hpp"
#include "rcspp/algorithm/simple_dominance_algorithm.hpp"
#include "rcspp/resource/concrete/container_resource.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "resource_types.hpp"

namespace py = pybind11;

using namespace rcspp;

// ─── Concrete type aliases ────────────────────────────────────────────────────

using RealRC = ResourceComposition<RealResource>;
using RealGraph = Graph<RealRC>;
using RealRG = ResourceGraph<RealResource>;

using RealIntRC = ResourceComposition<RealResource, IntResource>;
using RealIntRG = ResourceGraph<RealResource, IntResource>;

// Universal graph — covers all Python-meaningful types; used as fallback when no
// narrower binding exists for a requested combination.
using AllRC = ResourceComposition<RealResource, IntResource, RealSetResource, IntSetResource,
                                  UIntBitsetResource>;
using AllRG =
    ResourceGraph<RealResource, IntResource, RealSetResource, IntSetResource, UIntBitsetResource>;

// ─── Algorithm dispatch table ─────────────────────────────────────────────────
// Add new algorithms here only; dispatch is driven automatically.

enum class SolverAlgorithm { Simple, Pulling, Greedy };

// Associates one SolverAlgorithm enum value with its algorithm template.
template <SolverAlgorithm E, template <typename> class Algo>
struct AlgoEntry {
        static constexpr SolverAlgorithm value = E;
        template <typename RG, typename CostRC>
        static std::vector<Solution> run(RG& rg, double ub, AlgorithmParams p, bool pre, int ci) {
            return rg.template solve<Algo, CostRC>(ub, p, pre, ci);
        }
};

// Single source of truth for the enum → algorithm template mapping.
using AlgorithmTable = std::tuple<AlgoEntry<SolverAlgorithm::Simple, SimpleDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Pulling, PullingDominanceAlgorithm>,
                                  AlgoEntry<SolverAlgorithm::Greedy, GreedyAlgorithm>>;

template <typename RG, typename CostRC, typename... Entries>
std::vector<Solution> dispatch_algorithm_impl(SolverAlgorithm alg, RG& rg, double ub,
                                              AlgorithmParams p, bool pre, int ci,
                                              std::tuple<Entries...>* /*tag*/) {
    std::vector<Solution> result;
    [[maybe_unused]] bool matched =
        ((Entries::value == alg
              ? (result = Entries::template run<RG, CostRC>(rg, ub, p, pre, ci), true)
              : false) ||
         ...);
    return result;
}

template <typename RG, typename CostRC>
std::vector<Solution> dispatch_algorithm(SolverAlgorithm alg, RG& rg, double ub, AlgorithmParams p,
                                         bool pre, int ci) {
    return dispatch_algorithm_impl<RG, CostRC>(alg,
                                               rg,
                                               ub,
                                               p,
                                               pre,
                                               ci,
                                               static_cast<AlgorithmTable*>(nullptr));
}

// ─── Resource type → pybind11 method name ────────────────────────────────────
// Generated for all resource types via X-macro.

template <typename T>
constexpr const char* add_resource_method_name();

#define GEN_ADD_RESOURCE_NAME(name, scalar, RT)            \
    template <>                                            \
    constexpr const char* add_resource_method_name<RT>() { \
        return "add_" #name "_resource";                   \
    }
RCSPP_ALL_RESOURCES(GEN_ADD_RESOURCE_NAME)
#undef GEN_ADD_RESOURCE_NAME

// ─── Helper: bind common Graph base methods ───────────────────────────────────

template <typename G>
py::class_<G>& bind_graph_methods(py::class_<G>& c) {
    return c.def("get_node", &G::get_node, py::arg("id"), py::return_value_policy::reference)
        .def("get_arc", &G::get_arc, py::arg("id"), py::return_value_policy::reference)
        .def("node_ids", &G::get_node_ids)
        .def("arc_ids", &G::get_arc_ids)
        .def("source_node_ids", &G::get_source_node_ids)
        .def("sink_node_ids", &G::get_sink_node_ids)
        .def("number_of_nodes", &G::get_number_of_nodes)
        .def("number_of_arcs", &G::get_number_of_arcs)
        .def("is_source", &G::is_source, py::arg("node_id"))
        .def("is_sink", &G::is_sink, py::arg("node_id"));
}

// ─── Helper: bind common ResourceGraph methods (algorithm dispatch included) ──

template <typename RG, typename RC, typename CostRC = RealResource>
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
            [](RG& rg, SolverAlgorithm alg, double ub, AlgorithmParams p, bool pre, int ci) {
                return dispatch_algorithm<RG, CostRC>(alg, rg, ub, p, pre, ci);
            },
            py::arg("algorithm") = SolverAlgorithm::Simple,
            py::arg("upper_bound") = INF,
            py::arg("params") = AlgorithmParams{},
            py::arg("preprocess") = true,
            py::arg("cost_index") = 0)
        .def("process_feasibility", &RG::process_feasibility)
        .def("is_connected",
             &RG::is_connected,
             py::arg("origin_node_id"),
             py::arg("destination_node_id"));
}

// ─── Helper: bind one add_resource method for a specific resource type ────────

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

// ─── Helper: bind resource-specific methods (add_resource*, add_arc, ──────────
//             update_arc, and update_reduced_costs when RealResource is present)

template <typename RG, typename RC, typename... ResourceTypes>
void bind_resource_graph_impl(py::class_<RG, Graph<RC>>& rg) {
    using AddArcTuple = std::tuple<std::vector<ResourceInitializerTypeTuple_t<ResourceTypes>>...>;

    // add_<type>_resource — one per resource type via fold
    (bind_add_resource<RG, RC, ResourceTypes>(rg), ...);

    rg.def("add_arc",
           static_cast<Arc<RC>& (RG::*)(const AddArcTuple&,
                                        size_t,
                                        size_t,
                                        std::optional<size_t>,
                                        double,
                                        std::vector<Row>)>(&RG::add_arc),
           py::arg("resource_consumption"),
           py::arg("origin_node_id"),
           py::arg("destination_node_id"),
           py::arg("id") = std::nullopt,
           py::arg("cost") = 0.0,
           py::arg("dual_rows") = std::vector<Row>{},
           py::return_value_policy::reference);

    rg.def("update_arc",
           static_cast<void (RG::*)(Arc<RC>*, const AddArcTuple&, std::optional<double>)>(
               &RG::update_arc),
           py::arg("arc"),
           py::arg("resource_consumption"),
           py::arg("cost") = std::nullopt);

    // update_reduced_costs is only meaningful when RealResource is present
    if constexpr ((std::is_same_v<ResourceTypes, RealResource> || ...)) {
        rg.def(
            "update_reduced_costs",
            [](RG& rg, const std::vector<double>& duals, size_t cost_index) {
                rg.template update_reduced_costs<RealResource>(duals, cost_index);
            },
            py::arg("duals"),
            py::arg("cost_index") = 0);
    }
}

// ─── Helper: build the full block for a set of resource types ─────────────────
// Registers Node, Arc, plain Graph, and ResourceGraph under the given Python names.

template <typename RG, typename RC, typename CostRC, typename... ResourceTypes>
void bind_resource_graph_block(py::module_& m, const char* rg_name, const char* graph_name,
                               const char* node_name, const char* arc_name) {
    py::class_<Node<RC>>(m, node_name)
        .def_readonly("id", &Node<RC>::id)
        .def_readonly("source", &Node<RC>::source)
        .def_readonly("sink", &Node<RC>::sink);

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
        .def_readwrite("dual_rows", &Arc<RC>::dual_rows);

    {
        py::class_<Graph<RC>> g(m, graph_name);
        bind_graph_methods(g);
    }

    py::class_<RG, Graph<RC>> rg(m, rg_name);
    bind_rg_methods<RG, RC, CostRC>(rg);
    rg.def(py::init<>());
    bind_resource_graph_impl<RG, RC, ResourceTypes...>(rg);
}

// ─── Macros: bind single-resource graph blocks ───────────────────────────────
// Numerical: CostRC = RT (real/int/uint have a meaningful cost resource).
// Container: CostRC = RealResource (containers don't have a numeric cost resource;
//            RealResource satisfies requires, and the "not in pack" guard skips preprocessing).

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

// ─── init_graph ───────────────────────────────────────────────────────────────

void init_graph(py::module_& m) {
    // ── Algorithm enum ────────────────────────────────────────────────────────

    py::enum_<SolverAlgorithm>(m, "Algorithm")
        .value("Simple", SolverAlgorithm::Simple)
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

    py::class_<AlgorithmParams>(m, "AlgorithmParams")
        .def(py::init<>())
        .def("check", &AlgorithmParams::check, py::return_value_policy::reference_internal)
        .def("could_be_non_optimal", &AlgorithmParams::could_be_non_optimal)
        .def_readwrite("stop_after_X_solutions", &AlgorithmParams::stop_after_X_solutions)
        .def_readwrite("return_dominated_solutions", &AlgorithmParams::return_dominated_solutions)
        .def_readwrite("use_pool", &AlgorithmParams::use_pool)
        .def_readwrite("num_labels_to_extend_by_node",
                       &AlgorithmParams::num_labels_to_extend_by_node)
        .def_readwrite("num_max_phases", &AlgorithmParams::num_max_phases)
        .def_readwrite("max_iterations", &AlgorithmParams::max_iterations)
        .def_readwrite("tabu_tenure", &AlgorithmParams::tabu_tenure)
        .def_readwrite("forbidden_tabu", &AlgorithmParams::forbidden_tabu)
        .def_readwrite("tabu_random_noise", &AlgorithmParams::tabu_random_noise)
        .def_readwrite("seed", &AlgorithmParams::seed);

    py::class_<Solution>(m, "Solution")
        .def(py::init<>())
        .def_readwrite("cost", &Solution::cost)
        .def_readwrite("path_node_ids", &Solution::path_node_ids)
        .def_readwrite("path_arc_ids", &Solution::path_arc_ids);

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
                 py::overload_cast<Node<RealRC>*,
                                   Node<RealRC>*,
                                   std::optional<size_t>,
                                   double,
                                   std::vector<Row>>(&RealGraph::add_arc),
                 py::arg("origin"),
                 py::arg("destination"),
                 py::arg("id") = std::nullopt,
                 py::arg("cost") = 0.0,
                 py::arg("dual_rows") = std::vector<Row>{},
                 py::return_value_policy::reference)
            .def("add_arc",
                 py::overload_cast<size_t, size_t, std::optional<size_t>, double, std::vector<Row>>(
                     &RealGraph::add_arc),
                 py::arg("origin_id"),
                 py::arg("destination_id"),
                 py::arg("id") = std::nullopt,
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
        .def_readwrite("resource", &Node<RealRC>::resource);

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
        .def_readwrite("dual_rows", &Arc<RealRC>::dual_rows);

    {
        py::class_<RealRG, RealGraph> rg(m, "_real_resource_graph");
        bind_rg_methods<RealRG, RealRC>(rg);
        rg.def(py::init<>());
        bind_resource_graph_impl<RealRG, RealRC, RealResource>(rg);
    }

    // ══════════════════════════════════════════════════════════════════════════
    // All other single-resource graphs — generated via X-macro
    // ══════════════════════════════════════════════════════════════════════════

    BIND_SINGLE_NUMERICAL_RG(int, int, IntResource)
    BIND_SINGLE_NUMERICAL_RG(uint, unsigned int, UIntResource)
    BIND_SINGLE_CONTAINER_RG(real_set, double, RealSetResource)
    BIND_SINGLE_CONTAINER_RG(int_set, int, IntSetResource)
    BIND_SINGLE_CONTAINER_RG(uint_set, unsigned int, UIntSetResource)
    BIND_SINGLE_CONTAINER_RG(size_t_set, size_t, SizeTSetResource)
    BIND_SINGLE_CONTAINER_RG(uint_bitset, unsigned int, UIntBitsetResource)
    BIND_SINGLE_CONTAINER_RG(size_t_bitset, size_t, SizeTBitsetResource)

    // ══════════════════════════════════════════════════════════════════════════
    // Mixed graphs (common combinations)
    // ══════════════════════════════════════════════════════════════════════════

    bind_resource_graph_block<RealIntRG, RealIntRC, RealResource, RealResource, IntResource>(
        m,
        "_real_int_resource_graph",
        "_real_int_graph",
        "_real_int_node",
        "_real_int_arc");

    bind_resource_graph_block<AllRG,
                              AllRC,
                              RealResource,
                              RealResource,
                              IntResource,
                              RealSetResource,
                              IntSetResource,
                              UIntBitsetResource>(m,
                                                  "_all_resource_graph",
                                                  "_all_graph",
                                                  "_all_node",
                                                  "_all_arc");
}
