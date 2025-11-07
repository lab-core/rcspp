// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>

#include "rcspp/resource/base/resource_factory.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/functions/cost/real_value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/real_value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/expansion/real_addition_expansion_function.hpp"
#include "rcspp/resource/concrete/functions/expansion/time_window_expansion_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/time_window_feasibility_function.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
#include "rcspp/resource/concrete/real_resource_factory.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"

namespace py = pybind11;

using namespace rcspp;

using ResourceCompositionBase = ResourceComposition<RealResource>;
using ResourceCompositionFactoryBase = ResourceCompositionFactory<RealResource>;

using ConcreteResource = Resource<ResourceCompositionBase>;
using ConcreteFactory = ResourceFactory<ResourceCompositionBase>;

using RealResourceFactoryBase = ResourceFactory<RealResource>;

using RealExpansionFunction = ExpansionFunction<RealResource>;
using RealFeasibilityFunction = FeasibilityFunction<RealResource>;
using RealCostFunction = CostFunction<RealResource>;
using RealDominanceFunction = DominanceFunction<RealResource>;

void init_resource(py::module_& m) {
    // Resources

    py::class_<ConcreteResource, py::smart_holder>(m, "ConcreteResource").def(py::init<>());

    // Resource factories

    py::class_<RealResourceFactory>(m, "RealResourceFactory")
        .def(py::init<>())
        .def(py::init<const RealResource&>(), py::arg("real_resource_prototype"))
        .def(py::init<std::unique_ptr<RealExpansionFunction>,
                      std::unique_ptr<RealFeasibilityFunction>,
                      std::unique_ptr<RealCostFunction>,
                      std::unique_ptr<RealDominanceFunction>>(),
             py::arg("expansion_function"),
             py::arg("feasibility_function"),
             py::arg("cost_function"),
             py::arg("dominance_function"))
        .def(py::init<const RealResource&,
                      std::unique_ptr<RealExpansionFunction>,
                      std::unique_ptr<RealFeasibilityFunction>,
                      std::unique_ptr<RealCostFunction>,
                      std::unique_ptr<RealDominanceFunction>>(),
             py::arg("real_resource_prototype"),
             py::arg("expansion_function"),
             py::arg("feasibility_function"),
             py::arg("cost_function"),
             py::arg("dominance_function"));

    // Resource functions

    // Abstract resource functions

    py::class_<CostFunction<RealResource>, py::smart_holder>(m, "CostFunctionRealResource");

    py::class_<DominanceFunction<RealResource>, py::smart_holder>(m,
                                                                  "DominanceFunctionRealResource");

    py::class_<ExpansionFunction<RealResource>, py::smart_holder>(m,
                                                                  "ExpansionFunctionRealResource");

    py::class_<FeasibilityFunction<RealResource>, py::smart_holder>(
        m,
        "FeasibilityFunctionRealResource");

    // Concrete resource functions

    py::class_<RealValueCostFunction, CostFunction<RealResource>, py::smart_holder>(
        m,
        "RealValueCostFunction")
        .def(py::init<>());

    py::class_<RealValueDominanceFunction, DominanceFunction<RealResource>, py::smart_holder>(
        m,
        "RealValueDominanceFunction")
        .def(py::init<>());

    py::class_<RealAdditionExpansionFunction, ExpansionFunction<RealResource>, py::smart_holder>(
        m,
        "RealAdditionExpansionFunction")
        .def(py::init<>());

    py::class_<MinMaxFeasibilityFunction, FeasibilityFunction<RealResource>, py::smart_holder>(
        m,
        "MinMaxFeasibilityFunction")
        .def(py::init<double, double>());

    static std::map<size_t, double> g_min_time_window_by_arc_id;

    py::class_<TimeWindowExpansionFunction, ExpansionFunction<RealResource>, py::smart_holder>(
        m,
        "TimeWindowExpansionFunction")
        .def(py::init([](const py::dict& min_time_window_by_arc_id) {
                 // Copy Python dict into the global map
                 g_min_time_window_by_arc_id.clear();
                 for (const auto& [node_id, max_time] : min_time_window_by_arc_id) {
                     g_min_time_window_by_arc_id.emplace(node_id.cast<size_t>(),
                                                         max_time.cast<double>());
                 }
                 // Return an object referencing the global map
                 return TimeWindowExpansionFunction(g_min_time_window_by_arc_id);
             }),
             py::arg("min_time_window_by_arc_id"));

    static std::map<size_t, double> g_max_time_window_by_node_id;

    py::class_<TimeWindowFeasibilityFunction, FeasibilityFunction<RealResource>, py::smart_holder>(
        m,
        "TimeWindowFeasibilityFunction")
        .def(py::init([](const py::dict& max_time_window_by_node_id) {
                 // Copy Python dict into the global map
                 g_max_time_window_by_node_id.clear();
                 for (const auto& [node_id, max_time] : max_time_window_by_node_id) {
                     g_max_time_window_by_node_id.emplace(node_id.cast<size_t>(),
                                                          max_time.cast<double>());
                 }
                 // Return an object referencing the global map
                 return TimeWindowFeasibilityFunction(g_max_time_window_by_node_id);
             }),
             py::arg("max_time_window_by_node_id"));

    py::class_<TrivialFeasibilityFunction<RealResource>,
               FeasibilityFunction<RealResource>,
               py::smart_holder>(m, "RealTrivialFeasibilityFunction")
        .def(py::init<>());
}
