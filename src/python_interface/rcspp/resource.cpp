// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>

#include "rcspp/resource/composition/resource_composition_factory.hpp"
#include "rcspp/resource/concrete/functions/cost/value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/extension/addition_extension_function.hpp"
#include "rcspp/resource/concrete/functions/extension/time_window_extension_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/time_window_feasibility_function.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace py = pybind11;

using namespace rcspp;

using ResourceCompositionBase = ResourceBaseComposition<RealResource>;
using ResourceCompositionFactoryBase = ResourceCompositionFactory<RealResource>;

using ConcreteResource = Resource<ResourceCompositionBase>;
using ConcreteFactory = ResourceFactory<ResourceCompositionBase>;

using RealResourceFactoryBase = ResourceFactory<RealResource>;

using RealExtensionFunction = ExtensionFunction<RealResource>;
using RealFeasibilityFunction = FeasibilityFunction<RealResource>;
using RealCostFunction = CostFunction<RealResource>;
using RealDominanceFunction = DominanceFunction<RealResource>;

void init_resource(py::module_& m) {
    // Resources

    py::class_<ConcreteResource, py::smart_holder>(m, "ConcreteResource").def(py::init<>());

    // Resource factories

    py::class_<ResourceFactory<RealResource>>(m, "RealResourceFactory")
        .def(py::init<>())
        .def(py::init<std::unique_ptr<RealExtensionFunction>,
                      std::unique_ptr<RealFeasibilityFunction>,
                      std::unique_ptr<RealCostFunction>,
                      std::unique_ptr<RealDominanceFunction>>(),
             py::arg("extension_function"),
             py::arg("feasibility_function"),
             py::arg("cost_function"),
             py::arg("dominance_function"))
        .def(py::init<std::unique_ptr<RealExtensionFunction>,
                      std::unique_ptr<RealFeasibilityFunction>,
                      std::unique_ptr<RealCostFunction>,
                      std::unique_ptr<RealDominanceFunction>,
                      const RealResource&>(),
             py::arg("extension_function"),
             py::arg("feasibility_function"),
             py::arg("cost_function"),
             py::arg("dominance_function"),
             py::arg("real_resource_prototype"));

    // Resource functions

    // Abstract resource functions

    py::class_<CostFunction<RealResource>, py::smart_holder>(m, "CostFunctionRealResource");

    py::class_<DominanceFunction<RealResource>, py::smart_holder>(m,
                                                                  "DominanceFunctionRealResource");

    py::class_<ExtensionFunction<RealResource>, py::smart_holder>(m,
                                                                  "ExtensionFunctionRealResource");

    py::class_<FeasibilityFunction<RealResource>, py::smart_holder>(
        m,
        "FeasibilityFunctionRealResource");

    // Concrete resource functions

    py::class_<ValueCostFunction<RealResource>, CostFunction<RealResource>, py::smart_holder>(
        m,
        "RealValueCostFunction")
        .def(py::init<>());

    py::class_<ValueDominanceFunction<RealResource>,
               DominanceFunction<RealResource>,
               py::smart_holder>(m, "RealValueDominanceFunction")
        .def(py::init<>());

    py::class_<AdditionExtensionFunction<RealResource>,
               ExtensionFunction<RealResource>,
               py::smart_holder>(m, "RealAdditionExtensionFunction")
        .def(py::init<>());

    py::class_<MinMaxFeasibilityFunction<RealResource>,
               FeasibilityFunction<RealResource>,
               py::smart_holder>(m, "MinMaxFeasibilityFunction")
        .def(py::init<double, double>());

    static std::map<size_t, std::pair<double, double>> g_e_time_window_by_node_id;

    py::class_<TimeWindowExtensionFunction<RealResource>,
               ExtensionFunction<RealResource>,
               py::smart_holder>(m, "TimeWindowExtensionFunction")
        .def(py::init([](const py::dict& time_window_by_node_id) {
                 // Copy Python dict into the global map
                 g_e_time_window_by_node_id.clear();
                 for (const auto& [node_id, time_pair] : time_window_by_node_id) {
                     g_e_time_window_by_node_id.emplace(
                         node_id.cast<size_t>(),
                         time_pair.cast<std::pair<double, double>>());
                 }
                 // Return an object referencing the global map
                 return TimeWindowExtensionFunction<RealResource>(g_e_time_window_by_node_id);
             }),
             py::arg("time_window_by_arc_id"));

    static std::map<size_t, std::pair<double, double>> g_f_time_window_by_node_id;

    py::class_<TimeWindowFeasibilityFunction<RealResource>,
               FeasibilityFunction<RealResource>,
               py::smart_holder>(m, "TimeWindowFeasibilityFunction")
        .def(py::init([](const py::dict& max_time_window_by_node_id) {
                 // Copy Python dict into the global map
                 g_f_time_window_by_node_id.clear();
                 for (const auto& [node_id, time_pair] : max_time_window_by_node_id) {
                     g_f_time_window_by_node_id.emplace(
                         node_id.cast<size_t>(),
                         time_pair.cast<std::pair<double, double>>());
                 }
                 // Return an object referencing the global map
                 return TimeWindowFeasibilityFunction<RealResource>(g_f_time_window_by_node_id);
             }),
             py::arg("time_window_by_node_id"));

    py::class_<TrivialFeasibilityFunction<RealResource>,
               FeasibilityFunction<RealResource>,
               py::smart_holder>(m, "RealTrivialFeasibilityFunction")
        .def(py::init<>());
}
