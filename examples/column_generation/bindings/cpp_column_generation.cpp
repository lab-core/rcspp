// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The example's submodule of rcspp's Python module, rcspp._core.cpp_column_generation: the layer,
// and its HiGHS master solver when built.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>

namespace py = pybind11;

void init_column_generation(py::module_&);
#ifdef RCSPP_USE_COLUMN_GENERATION_EXAMPLE_HIGHS
void init_column_generation_highs(py::module_&);
#endif

// Called by rcspp's module (python/bindings/rcspp.cpp), after its graph submodule.
void init_cpp_column_generation(py::module_& m) {
    auto submodule = m.def_submodule("cpp_column_generation",
                                     "C++ column generation example: master, pricers and loop");
    init_column_generation(submodule);
#ifdef RCSPP_USE_COLUMN_GENERATION_EXAMPLE_HIGHS
    init_column_generation_highs(submodule);
#endif
}
