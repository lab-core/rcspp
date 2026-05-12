// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <map>

#include "rcspp/rcspp.hpp"
#include "resource_types.hpp"

namespace py = pybind11;

using namespace rcspp;

// ── MapRef ────────────────────────────────────────────────────────────────────
// Owns a std::map<size_t, T> and exposes a const pointer so C++ objects that
// store raw map pointers (TimeWindowExtensionFunction, TimeWindowFeasibility-
// Function) can be built without aliasing or external lifetime dependencies.
//
// Usage pattern in pybind11:
//   .def(py::init([](const MapRef<T>& mr) { return Fn<RT>(mr.get()); }),
//        py::arg("map_ref"),
//        py::keep_alive<1, 2>())   // keep MapRef alive as long as the Fn object
//
// py::keep_alive<0, 1> means:
//   Nurse  = 1 → the newly constructed C++ function object
//   Patient = 2 → map_ref (first constructor argument)
// The patient (MapRef) lives at least until the nurse (function object) is freed.
// Combined with ResourceGraph._refs keeping the function object's Python wrapper
// alive, the map is guaranteed live for the entire graph lifetime.

template <typename T>
class MapRef final {
        std::map<size_t, T> map_;

    public:
        explicit MapRef(std::map<size_t, T> m) : map_(std::move(m)) {}

        [[nodiscard]] const std::map<size_t, T>* get() const { return &map_; }
};

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
    py::class_<MapRef<scalar>, py::smart_holder>(m, "MapRef_" #name)                       \
        .def(py::init([](const py::dict& d) {                                                   \
                 std::map<size_t, scalar> map;                                                  \
                 for (const auto& [k, v] : d) {                                                 \
                     map.emplace(k.cast<size_t>(), v.cast<scalar>());                          \
                 }                                                                               \
                 return MapRef<scalar>(std::move(map));                                     \
             }),                                                                                 \
             py::arg("map"));                                                                    \
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
        .def(py::init([](const MapRef<scalar>& map_ref) {                                   \
                 return TimeWindowExtensionFunction<RT>(map_ref.get());                         \
             }),                                                                                 \
             py::arg("map_ref"),                                                                 \
             py::keep_alive<1, 2>());                                                           \
    py::class_<TimeWindowFeasibilityFunction<RT>, FeasibilityFunction<RT>, py::smart_holder>(   \
        m, "TimeWindowFeasibilityFunction_" #name)                                              \
        .def(py::init([](const MapRef<scalar>& map_ref) {                                   \
                 return TimeWindowFeasibilityFunction<RT>(map_ref.get());                       \
             }),                                                                                 \
             py::arg("map_ref"),                                                                 \
             py::keep_alive<1, 2>());
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
