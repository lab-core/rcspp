// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include "rcspp/graph/graph.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>

#include "rcspp/resource/resource_graph.hpp"

namespace py = pybind11;

using namespace rcspp;

using ResourceCompositionBase = ResourceComposition<RealResource>;
using ResourceCompositionFactoryBase = ResourceCompositionFactory<RealResource>;

using ConcreteGraph = Graph<ResourceCompositionBase>;
using ConcreteNode = Node<ResourceCompositionBase>;
using ConcreteArc = Arc<ResourceCompositionBase>;

using ConcreteExpander = Expander<ResourceCompositionBase>;

void init_graph(py::module_& m) {
    py::class_<ConcreteGraph>(m, "Graph")
        .def(py::init<>())
        .def("add_node",
             &ConcreteGraph::add_node,
             py::arg("id"),
             py::arg("source") = false,
             py::arg("sink") = false,
             py::return_value_policy::reference)
        .def("add_arc",
             py::overload_cast<ConcreteNode*,
                               ConcreteNode*,
                               std::optional<size_t>,
                               double,
                               std::vector<Row>>(&ConcreteGraph::add_arc),
             py::arg("origin_node"),
             py::arg("destination_node"),
             py::arg("id") = std::nullopt,
             py::arg("cost") = 0.0,
             py::arg("dual_rows") = std::vector<Row>{},
             py::return_value_policy::reference)
        .def("add_arc",
             py::overload_cast<size_t, size_t, std::optional<size_t>, double, std::vector<Row>>(
                 &ConcreteGraph::add_arc),
             py::arg("origin_node_id"),
             py::arg("destination_node_id"),
             py::arg("id") = std::nullopt,
             py::arg("cost") = 0.0,
             py::arg("dual_rows") = std::vector<Row>{},
             py::return_value_policy::reference)
        .def("get_node",
             &ConcreteGraph::get_node,
             py::arg("id"),
             py::return_value_policy::reference)
        .def("get_arc", &ConcreteGraph::get_arc, py::arg("id"), py::return_value_policy::reference)
        .def("get_node_ids", &ConcreteGraph::get_node_ids)
        .def("get_arc_ids", &ConcreteGraph::get_arc_ids)
        .def("get_source_node_ids", &ConcreteGraph::get_source_node_ids)
        .def("get_sink_node_ids", &ConcreteGraph::get_sink_node_ids)
        .def("get_number_of_nodes", &ConcreteGraph::get_number_of_nodes)
        .def("get_number_of_arcs", &ConcreteGraph::get_number_of_arcs)
        .def("is_source", &ConcreteGraph::is_source, py::arg("node_id"))
        .def("is_sink", &ConcreteGraph::is_sink, py::arg("node_id"));

    py::class_<ConcreteNode>(m, "Node")
        .def(py::init<size_t, bool, bool>())
        .def_readonly("id", &ConcreteNode::id)
        .def("pos", &ConcreteNode::pos)
        .def_readonly("source", &ConcreteNode::source)
        .def_readonly("sink", &ConcreteNode::sink)
        .def_readwrite("in_arcs", &ConcreteNode::in_arcs)
        .def_readwrite("out_arcs", &ConcreteNode::out_arcs)
        .def_readwrite("resource", &ConcreteNode::resource);

    py::class_<ConcreteArc>(m, "Arc")
        .def(py::init<size_t,
                      ConcreteNode*,
                      ConcreteNode*,
                      std::unique_ptr<ConcreteExpander>,
                      double>(),
             py::arg("id"),
             py::arg("origin"),
             py::arg("destination"),
             py::arg("expander"),
             py::arg("cost"))
        .def(py::init<size_t, ConcreteNode*, ConcreteNode*>(),
             py::arg("id"),
             py::arg("origin"),
             py::arg("destination"))
        .def_readonly("id", &ConcreteArc::id)
        .def(
            "get_origin",
            [](const ConcreteArc& arc) -> ConcreteNode* { return arc.origin; },
            py::return_value_policy::reference)
        .def(
            "get_destination",
            [](const ConcreteArc& arc) -> ConcreteNode* { return arc.destination; },
            py::return_value_policy::reference)
        .def_readwrite("expander", &ConcreteArc::expander)
        .def_readwrite("cost", &ConcreteArc::cost);

    py::class_<Solution>(m, "Solution")
        .def(py::init<>())
        .def_readwrite("cost", &Solution::cost)
        .def_readwrite("path_node_ids", &Solution::path_node_ids)
        .def_readwrite("path_arc_ids", &Solution::path_arc_ids);

    py::class_<ResourceGraph<RealResource>, ConcreteGraph>(m, "ResourceGraph")
        .def(py::init<>())
        .def("add_real_resource",
             &ResourceGraph<RealResource>::add_resource<RealResource>,
             py::arg("expansion_function"),
             py::arg("feasibility_function"),
             py::arg("cost_function"),
             py::arg("dominance_function"))
        .def("add_node",
             &ResourceGraph<RealResource>::add_node,
             py::arg("id"),
             py::arg("source") = false,
             py::arg("sink") = false,
             py::return_value_policy::reference)
        .def(
            "add_arc",
            static_cast<Arc<ResourceComposition<RealResource>>& (
                ResourceGraph<RealResource>::*)(const std::tuple<std::vector<
                                                    ResourceInitializerTypeTuple_t<RealResource>>>&,
                                                size_t,
                                                size_t,
                                                std::optional<size_t>,
                                                double,
                                                std::vector<Row>)>(
                &ResourceGraph<RealResource>::add_arc),
            py::arg("resource_consumption"),
            py::arg("origin_node_id"),
            py::arg("destination_node_id"),
            py::arg("id") = std::nullopt,
            py::arg("cost") = 0.0,
            py::arg("dual_rows") = std::vector<Row>{},
            py::return_value_policy::reference)
        .def("update_arc",
             static_cast<void (ResourceGraph<RealResource>::*)(
                 Arc<ResourceComposition<RealResource>>*,
                 const std::tuple<std::vector<ResourceInitializerTypeTuple_t<RealResource>>>&,
                 std::optional<double>
                     cost)>(&ResourceGraph<RealResource>::update_arc),
             py::arg("arc"),
             py::arg("resource_consumption"),
             py::arg("cost") = std::nullopt)
        .def("get_resource_factory",
             &ResourceGraph<RealResource>::get_resource_factory,
             py::return_value_policy::reference)
        .def("solve", &ResourceGraph<RealResource>::solve<SimpleDominanceAlgorithmIterators>);
}

// Macro pour add_resource
