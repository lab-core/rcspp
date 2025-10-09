// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void init_graph(py::module_&);
void init_resource(py::module_&);

PYBIND11_MODULE(rcsppy, m) {
    m.doc() = "RCSPP module";

    auto graph_submodule = m.def_submodule("graph", "Graph-related classes");
    init_graph(graph_submodule);

    auto resource_submodule = m.def_submodule("resource", "Resource-related classes");
    init_resource(resource_submodule);
}
