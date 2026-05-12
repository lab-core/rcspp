// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <map>
#include <memory>

#include "rcspp/rcspp.hpp"
#include "resource_types.hpp"

namespace py = pybind11;

using namespace rcspp;

void init_resource(py::module_& m) {
    // ── Abstract bases for every resource type ────────────────────────────────
    // Registered first so derived classes can reference them.

#define BIND_ABSTRACT_BASES(name, scalar, RT)                                               \
    py::class_<ExtensionFunction<RT>, py::smart_holder>(m, "ExtensionFunction_" #name);     \
    py::class_<FeasibilityFunction<RT>, py::smart_holder>(m, "FeasibilityFunction_" #name); \
    py::class_<CostFunction<RT>, py::smart_holder>(m, "CostFunction_" #name);               \
    py::class_<DominanceFunction<RT>, py::smart_holder>(m, "DominanceFunction_" #name);
    RCSPP_ALL_RESOURCES(BIND_ABSTRACT_BASES)
#undef BIND_ABSTRACT_BASES

    // ── Concrete functions for numerical resources ────────────────────────────

#define BIND_NUMERICAL_FUNCTIONS(name, scalar, RT)                                         \
    py::class_<AdditionExtensionFunction<RT>, ExtensionFunction<RT>, py::smart_holder>(    \
        m,                                                                                 \
        "AdditionExtensionFunction_" #name)                                                \
        .def(py::init<>());                                                                \
    py::class_<ValueCostFunction<RT>, CostFunction<RT>, py::smart_holder>(                 \
        m,                                                                                 \
        "ValueCostFunction_" #name)                                                        \
        .def(py::init<>());                                                                \
    py::class_<TrivialCostFunction<RT>, CostFunction<RT>, py::smart_holder>(               \
        m,                                                                                 \
        "TrivialCostFunction_" #name)                                                      \
        .def(py::init<>());                                                                \
    py::class_<ValueDominanceFunction<RT>, DominanceFunction<RT>, py::smart_holder>(       \
        m,                                                                                 \
        "ValueDominanceFunction_" #name)                                                   \
        .def(py::init<>());                                                                \
    py::class_<MinMaxFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(  \
        m,                                                                                 \
        "MinMaxFeasibilityFunction_" #name)                                                \
        .def(py::init<scalar, scalar>(), py::arg("min_value"), py::arg("max_value"));      \
    py::class_<TrivialFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>( \
        m,                                                                                 \
        "TrivialFeasibilityFunction_" #name)                                               \
        .def(py::init<>());
    RCSPP_NUMERICAL_RESOURCES(BIND_NUMERICAL_FUNCTIONS)
#undef BIND_NUMERICAL_FUNCTIONS

    // ── Concrete functions for container resources ────────────────────────────

#define BIND_CONTAINER_FUNCTIONS(name, scalar, RT)                                          \
    py::class_<TrivialCostFunction<RT>, CostFunction<RT>, py::smart_holder>(                \
        m,                                                                                  \
        "TrivialCostFunction_" #name)                                                       \
        .def(py::init<>());                                                                 \
    py::class_<TrivialFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(  \
        m,                                                                                  \
        "TrivialFeasibilityFunction_" #name)                                                \
        .def(py::init<>());                                                                 \
    py::class_<InclusionDominanceFunction<RT>, DominanceFunction<RT>, py::smart_holder>(    \
        m,                                                                                  \
        "InclusionDominanceFunction_" #name)                                                \
        .def(py::init<>());                                                                 \
    py::class_<ContainDominanceFunction<RT>, DominanceFunction<RT>, py::smart_holder>(      \
        m,                                                                                  \
        "ContainDominanceFunction_" #name)                                                  \
        .def(py::init<>());                                                                 \
    py::class_<UnionExtensionFunction<RT>, ExtensionFunction<RT>, py::smart_holder>(        \
        m,                                                                                  \
        "UnionExtensionFunction_" #name)                                                    \
        .def(py::init<>());                                                                 \
    py::class_<IntersectionExtensionFunction<RT>, ExtensionFunction<RT>, py::smart_holder>( \
        m,                                                                                  \
        "IntersectionExtensionFunction_" #name)                                             \
        .def(py::init<>());                                                                 \
    py::class_<SubtractExtensionFunction<RT>, ExtensionFunction<RT>, py::smart_holder>(     \
        m,                                                                                  \
        "SubtractExtensionFunction_" #name)                                                 \
        .def(py::init<>());                                                                 \
    py::class_<SizeFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(     \
        m,                                                                                  \
        "SizeFeasibilityFunction_" #name)                                                   \
        .def(py::init<size_t, size_t>(), py::arg("min_size"), py::arg("max_size"));
    RCSPP_CONTAINER_RESOURCES(BIND_CONTAINER_FUNCTIONS)
#undef BIND_CONTAINER_FUNCTIONS

    // ── Real-only: time-window functions ──────────────────────────────────────
    // These use node-ID maps; not generalisable to other numeric types.

    py::class_<TimeWindowExtensionFunction<RealResource>,
               ExtensionFunction<RealResource>,
               py::smart_holder>(m, "TimeWindowExtensionFunction")
        .def(py::init([](const py::dict& min_tw_by_node) {
                 std::map<size_t, double> map;
                 for (const auto& [k, v] : min_tw_by_node) {
                     map.emplace(k.cast<size_t>(), v.cast<double>());
                 }
                 return TimeWindowExtensionFunction<RealResource>(std::move(map));
             }),
             py::arg("min_time_window_by_node_id"));

    py::class_<TimeWindowFeasibilityFunction<RealResource>,
               FeasibilityFunction<RealResource>,
               py::smart_holder>(m, "TimeWindowFeasibilityFunction")
        .def(py::init([](const py::dict& max_tw_by_node) {
                 std::map<size_t, double> map;
                 for (const auto& [k, v] : max_tw_by_node) {
                     map.emplace(k.cast<size_t>(), v.cast<double>());
                 }
                 return TimeWindowFeasibilityFunction<RealResource>(std::move(map));
             }),
             py::arg("max_time_window_by_node_id"));

    // ── Backward-compat aliases ───────────────────────────────────────────────

    m.attr("RealAdditionExtensionFunction") = m.attr("AdditionExtensionFunction_real");
    m.attr("RealValueCostFunction") = m.attr("ValueCostFunction_real");
    m.attr("RealTrivialFeasibilityFunction") = m.attr("TrivialFeasibilityFunction_real");
    m.attr("RealValueDominanceFunction") = m.attr("ValueDominanceFunction_real");
    m.attr("MinMaxFeasibilityFunction") = m.attr("MinMaxFeasibilityFunction_real");

    m.attr("IntAdditionExtensionFunction") = m.attr("AdditionExtensionFunction_int");
    m.attr("IntValueCostFunction") = m.attr("ValueCostFunction_int");
    m.attr("IntMinMaxFeasibilityFunction") = m.attr("MinMaxFeasibilityFunction_int");
    m.attr("IntTrivialFeasibilityFunction") = m.attr("TrivialFeasibilityFunction_int");
    m.attr("IntValueDominanceFunction") = m.attr("ValueDominanceFunction_int");

    m.attr("RealAdditionExpansionFunction") = m.attr("AdditionExtensionFunction_real");
    m.attr("TimeWindowExpansionFunction") = m.attr("TimeWindowExtensionFunction");
}
