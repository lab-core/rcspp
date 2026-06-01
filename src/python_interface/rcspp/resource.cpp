// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <limits>
#include <map>

#include "rcspp/rcspp.hpp"
#include "resource_types.hpp"

namespace py = pybind11;

using namespace rcspp;

// ─────────────────────────────────────────────────────────────────────────────

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
    // MapRef_<name> and both time-window classes are included here so they are
    // automatically registered for every numerical resource type.

    // clang-format off
#define BIND_NUMERICAL_FUNCTIONS(name, scalar, RT)                                              \
    py::class_<AdditionExtensionFunction<RT>, ExtensionFunction<RT>, py::smart_holder>(         \
        m, "AdditionExtensionFunction_" #name)                                                  \
        .def(py::init<>());                                                                     \
    py::class_<ValueCostFunction<RT>, CostFunction<RT>, py::smart_holder>(                      \
        m, "ValueCostFunction_" #name)                                                          \
        .def(py::init<>());                                                                     \
    py::class_<TrivialCostFunction<RT>, CostFunction<RT>, py::smart_holder>(                    \
        m, "TrivialCostFunction_" #name)                                                        \
        .def(py::init<>());                                                                     \
    py::class_<ValueDominanceFunction<RT>, DominanceFunction<RT>, py::smart_holder>(            \
        m, "ValueDominanceFunction_" #name)                                                     \
        .def(py::init<>());                                                                     \
    py::class_<MinMaxFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(       \
        m, "MinMaxFeasibilityFunction_" #name)                                                  \
        .def(py::init<scalar, scalar>(), py::arg("min_value"), py::arg("max_value"));           \
    py::class_<TrivialFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(      \
        m, "TrivialFeasibilityFunction_" #name)                                                 \
        .def(py::init<>());                                                                     \
    py::class_<TimeWindowExtensionFunction<RT>, ExtensionFunction<RT>, py::smart_holder>(       \
        m, "TimeWindowExtensionFunction_" #name)                                                \
        .def(py::init([](const py::dict& d, scalar default_max_value) {                        \
                 std::map<size_t, std::pair<scalar, scalar>> map;                               \
                 for (const auto& [k, v] : d) {                                                 \
                     auto tup = v.cast<py::tuple>();                                            \
                     map.emplace(k.cast<size_t>(),                                              \
                                 std::make_pair(tup[0].cast<scalar>(), tup[1].cast<scalar>())); \
                 }                                                                               \
                 return TimeWindowExtensionFunction<RT>(std::move(map), default_max_value);     \
             }),                                                                                 \
             py::arg("tw_by_node"),                                                              \
             py::arg("default_max_value") = std::numeric_limits<scalar>::max() / 2);            \
    py::class_<TimeWindowFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(   \
        m, "TimeWindowFeasibilityFunction_" #name)                                              \
        .def(py::init([](const py::dict& d, scalar default_max_value) {                        \
                 std::map<size_t, std::pair<scalar, scalar>> map;                               \
                 for (const auto& [k, v] : d) {                                                 \
                     auto tup = v.cast<py::tuple>();                                            \
                     map.emplace(k.cast<size_t>(),                                              \
                                 std::make_pair(tup[0].cast<scalar>(), tup[1].cast<scalar>())); \
                 }                                                                               \
                 return TimeWindowFeasibilityFunction<RT>(std::move(map), default_max_value);   \
             }),                                                                                 \
             py::arg("tw_by_node"),                                                              \
             py::arg("default_max_value") = std::numeric_limits<scalar>::max() / 2);
    // clang-format on
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
}
