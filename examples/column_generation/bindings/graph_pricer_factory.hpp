// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The graph pricer factory of each bound graph type. Included by rcspp's graph bindings
// (python/bindings/graph_impl.hpp) with USE_COLUMN_GENERATION_EXAMPLE: one factory per graph type
// with a real cost, registered when that type is bound.
//
// Not a header of its own: it compiles only at that include, after the names it takes from
// graph_impl.hpp. A change to any of them in the core breaks this file, with the example on only:
//   - the alias `py` and `using namespace rcspp`;
//   - PyAlgorithmParams (timeout_s, to_params<LabelContainerType>()) and
//     PyBucketAlgorithmParams (direction, timeout_s);
//   - run_bucket_solve<ResourceGraphType, CompositionType, CostResourceType,
//     ResourceTypes...>(algorithm, graph, upper_bound, params, preprocess, cost_index);
//   - dispatch_algorithm<ResourceGraphType, CostResourceType, LabelContainerType>(algorithm,
//     graph, upper_bound, params, preprocess, cost_index).
// graph_impl.hpp calls register_graph_pricer<ResourceGraphType, CompositionType,
// ResourceTypes...>(rg) when it binds a graph type (bind_resource_graph_impl).

#include <pybind11/pybind11.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "graph_pricer.hpp"
#include "rcspp/column_generation/graph_pricer.hpp"
#include "rcspp/column_generation/pricing_strategy.hpp"

// Builds the C++ GraphPricer over a bound graph from Python's stages: (Algorithm, params,
// SearchDirection, exact) tuples. Each stage searches as solve() does, with its own copy of the
// params, in its direction, with a time limit the request caps; the cost is the real resource,
// whose slot update_reduced_costs writes.

template <typename ResourceGraphType, typename CompositionType, typename... ResourceTypes>
std::unique_ptr<column_generation::Pricer> make_graph_pricer(ResourceGraphType& graph,
                                                             const py::list& stages,
                                                             size_t cost_index, bool preprocess) {
    using Stage = column_generation::PricingStage<ResourceTypes...>;
    using LabelContainerType = LabelList<CompositionType>;
    std::vector<Stage> cpp_stages;
    cpp_stages.reserve(stages.size());
    for (const py::handle stage : stages) {
        const auto [algorithm, params, direction, exact] =
            stage.cast<std::tuple<SolverAlgorithm, py::object, SearchDirection, bool>>();
        if (py::isinstance<PyBucketAlgorithmParams>(params)) {
            auto bucket_params = params.cast<PyBucketAlgorithmParams>();
            bucket_params.direction = direction;
            cpp_stages.emplace_back(
                [algorithm, bucket_params](ResourceGraphType& searched,
                                           const column_generation::StageRequest& request) {
                    PyBucketAlgorithmParams capped = bucket_params;
                    capped.timeout_s = std::min(capped.timeout_s, request.timeout_s);
                    return run_bucket_solve<ResourceGraphType,
                                            CompositionType,
                                            RealResource,
                                            ResourceTypes...>(algorithm,
                                                              searched,
                                                              request.upper_bound,
                                                              capped,
                                                              request.preprocess,
                                                              request.cost_index);
                },
                exact);
        } else {
            auto list_params =
                params.cast<PyAlgorithmParams>().template to_params<LabelContainerType>();
            list_params.direction = direction;
            cpp_stages.emplace_back(
                [algorithm, list_params](ResourceGraphType& searched,
                                         const column_generation::StageRequest& request) {
                    AlgorithmParams<LabelContainerType> capped = list_params;
                    capped.timeout_s = std::min(capped.timeout_s, request.timeout_s);
                    return dispatch_algorithm<ResourceGraphType, RealResource, LabelContainerType>(
                        algorithm,
                        searched,
                        request.upper_bound,
                        std::move(capped),
                        request.preprocess,
                        request.cost_index);
                },
                exact);
        }
    }
    return std::make_unique<column_generation::GraphPricer<ResourceTypes...>>(
        graph,
        column_generation::PricingStrategy<ResourceTypes...>(std::move(cpp_stages)),
        cost_index,
        preprocess);
}

// Registers the factory of `rg`'s graph type, found by the GraphPricer binding
// (column_generation.cpp) from the graph's Python type.
template <typename ResourceGraphType, typename CompositionType, typename... ResourceTypes>
void register_graph_pricer(py::class_<ResourceGraphType, Graph<CompositionType>>& rg) {
    graph_pricer_factories()[reinterpret_cast<PyTypeObject*>(rg.ptr())] =
        [](py::handle graph, const py::list& stages, size_t cost_index, bool preprocess) {
            return make_graph_pricer<ResourceGraphType, CompositionType, ResourceTypes...>(
                graph.cast<ResourceGraphType&>(),
                stages,
                cost_index,
                preprocess);
        };
}
