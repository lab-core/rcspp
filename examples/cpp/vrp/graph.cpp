// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "vrp/graph.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

namespace vrp {

namespace {

using rcspp::RealResource;
using rcspp::Row;
using rcspp::SizeTBitsetResource;

// Each customer's ng_size nearest customers, itself included; the depot and the sink none.
std::map<size_t, std::set<size_t>> ng_neighborhoods(const Instance& instance, size_t ng_size) {
    std::map<size_t, std::set<size_t>> neighborhoods;
    neighborhoods[instance.depot().number] = {};
    neighborhoods[sink(instance)] = {};

    for (const Customer& customer : instance.customers) {
        if (customer.number == instance.depot().number) {
            continue;
        }
        std::vector<std::pair<double, size_t>> nearest;
        for (const Customer& other : instance.customers) {
            if (other.number != instance.depot().number) {
                nearest.emplace_back(distance(customer, other), other.number);
            }
        }
        std::ranges::sort(nearest);
        std::set<size_t>& neighborhood = neighborhoods[customer.number];
        for (size_t index = 0; index < std::min(ng_size, nearest.size()); ++index) {
            neighborhood.insert(nearest[index].second);
        }
    }

    return neighborhoods;
}

}  // namespace

std::optional<Relaxation> parse_relaxation(std::string_view name) {
    if (name == "none") {
        return Relaxation::None;
    }
    if (name == "ng") {
        return Relaxation::NgPath;
    }
    if (name == "elementary") {
        return Relaxation::Elementary;
    }
    return std::nullopt;
}

size_t sink(const Instance& instance) {
    return instance.customers.size();
}

void pricing_graph(const Instance& instance, Graph& graph, Relaxation relaxation, size_t ng_size) {
    const Customer& depot = instance.depot();
    const size_t end = sink(instance);

    std::map<size_t, std::pair<double, double>> windows;
    for (const Customer& customer : instance.customers) {
        windows.emplace(customer.number, std::pair{customer.ready_time, customer.due_time});
    }

    // The route is back at the depot before it closes.
    windows[end] = windows[depot.number];

    // The resources: the cost first, then the route memory if any.
    rcspp::presets::add_cost_resource<RealResource>(graph);
    rcspp::presets::add_window_resource<RealResource>(graph, windows);
    rcspp::presets::add_capacity_resource<RealResource>(graph, instance.capacity);

    if (relaxation == Relaxation::NgPath) {
        rcspp::presets::add_ng_path_resource<SizeTBitsetResource>(
            graph,
            ng_neighborhoods(instance, ng_size));
    } else if (relaxation == Relaxation::Elementary) {
        std::vector<size_t> nodes;
        for (const Customer& customer : instance.customers) {
            nodes.push_back(customer.number);
        }
        nodes.push_back(end);
        rcspp::presets::add_elementary_resource<SizeTBitsetResource>(graph, nodes);
    }

    // The nodes.
    graph.add_node(depot.number, true);
    for (const Customer& customer : instance.customers) {
        if (customer.number != depot.number) {
            graph.add_node(customer.number);
        }
    }
    graph.add_node(end, false, true);

    // The arcs: from the depot or a customer to every other customer, and from every customer
    // to the sink. The depot's arcs cover no row. A route memory's value on an arc is empty:
    // its extension takes the node remembered from the arc's ends.
    const bool memory = relaxation != Relaxation::None;
    for (const Customer& origin : instance.customers) {
        const bool from_depot = origin.number == depot.number;
        std::vector<Row> rows;
        if (!from_depot) {
            rows.push_back(Row{.index = origin.number - 1, .coefficient = 1.0});
        }

        std::vector<std::pair<size_t, const Customer*>> destinations;
        for (const Customer& customer : instance.customers) {
            if (customer.number != depot.number && customer.number != origin.number) {
                destinations.emplace_back(customer.number, &customer);
            }
        }
        if (!from_depot) {
            destinations.emplace_back(end, &depot);
        }

        for (const auto& [node, destination] : destinations) {
            const double length = distance(origin, *destination);
            const double time = origin.service_time + length;
            if (memory) {
                graph.add_arc<RealResource, RealResource, RealResource, SizeTBitsetResource>(
                    std::make_tuple(std::make_tuple(length),
                                    std::make_tuple(time),
                                    std::make_tuple(destination->demand),
                                    std::make_tuple(std::set<size_t>{})),
                    origin.number,
                    node,
                    length,
                    rows);
            } else {
                graph.add_arc<RealResource, RealResource, RealResource>(
                    {length, time, destination->demand},
                    origin.number,
                    node,
                    length,
                    rows);
            }
        }
    }
}

}  // namespace vrp
