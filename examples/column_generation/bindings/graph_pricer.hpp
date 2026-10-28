// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The graph pricer as Python sees it: one class for every bound graph type. Each graph type that
// has a real cost registers a factory (graph_impl.hpp); the GraphPricer binding
// (column_generation.cpp) finds it by the graph's Python type.

#include <pybind11/pybind11.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>

#include "rcspp/column_generation/pricer.hpp"

namespace py = pybind11;

// A pricer over a graph of any bound type. It holds a C++ GraphPricer and forwards to it, so the
// loop prices without calling Python.
class AnyGraphPricer final : public rcspp::column_generation::Pricer {
    public:
        explicit AnyGraphPricer(std::unique_ptr<Pricer> pricer) : pricer_(std::move(pricer)) {}

        rcspp::column_generation::PricingResult price(
            std::span<const double> duals,
            const rcspp::column_generation::PricingRequest& request) override {
            return pricer_->price(duals, request);
        }

        void update(const rcspp::column_generation::PricingChanges& changes) override {
            pricer_->update(changes);
        }

        [[nodiscard]] double cost_scale() const override { return pricer_->cost_scale(); }

    private:
        std::unique_ptr<Pricer> pricer_;
};

// Builds the C++ GraphPricer over `graph`, a bound graph of the factory's type. `stages` holds
// one (Algorithm, AlgorithmParams or BucketAlgorithmParams, SearchDirection, exact) tuple per
// stage; the params are copied, so the search never reads a Python object.
using GraphPricerFactory = std::function<std::unique_ptr<rcspp::column_generation::Pricer>(
    py::handle graph, const py::list& stages, size_t cost_index, bool preprocess)>;

// The factory of every bound graph type that has a real cost, by its Python type.
inline std::unordered_map<PyTypeObject*, GraphPricerFactory>& graph_pricer_factories() {
    static std::unordered_map<PyTypeObject*, GraphPricerFactory> factories;
    return factories;
}
