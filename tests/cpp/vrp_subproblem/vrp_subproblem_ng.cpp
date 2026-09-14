// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "vrp_subproblem_ng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "rcspp/rcspp.hpp"

VRPSubproblemNg::VRPSubproblemNg(Instance instance, size_t ng_neighborhood_size)
    : instance_(std::move(instance)),
      ng_neighborhood_size_(ng_neighborhood_size),
      time_window_by_customer_id_(initialize_time_windows()),
      ng_neighborhood_by_customer_id_(initialize_ng_neighborhoods()) {
    LOG_TRACE("VRPSubproblemNg::VRPSubproblemNg\n");
    construct_resource_graph(&graph_);
}

std::map<size_t, std::pair<double, double>> VRPSubproblemNg::initialize_time_windows() {
    // Identical to VRPSubproblem's, deliberately: the two models must see the same windows or
    // their optima are not comparable.
    std::map<size_t, std::pair<double, double>> time_window_by_customer_id;

    const auto& customers_by_id = instance_.get_customers_by_id();
    for (const auto& [customer_id, customer] : customers_by_id) {
        time_window_by_customer_id.emplace(
            customer_id,
            std::pair<double, double>{customer.ready_time, customer.due_time});
    }

    time_window_by_customer_id.insert_or_assign(
        0,
        std::pair<double, double>{0, std::numeric_limits<double>::max() / 2});  // prevent overflow
    const size_t sink_id = customers_by_id.size();
    time_window_by_customer_id.insert_or_assign(
        sink_id,
        std::pair<double, double>{0, std::numeric_limits<double>::max() / 2});  // prevent overflow

    return time_window_by_customer_id;
}

std::map<size_t, std::set<size_t>> VRPSubproblemNg::initialize_ng_neighborhoods() {
    const auto& customers_by_id = instance_.get_customers_by_id();
    const size_t sink_id = customers_by_id.size();

    std::map<size_t, std::set<size_t>> neighborhoods;

    for (const auto& [customer_id, customer] : customers_by_id) {
        std::set<size_t> neighborhood;
        if (ng_neighborhood_size_ > 0 && !customer.depot) {
            // The classic ng-route choice: the N nearest other customers. Distances are computed
            // rather than cached because this runs once per model build.
            std::vector<std::pair<double, size_t>> by_distance;
            by_distance.reserve(customers_by_id.size());
            for (const auto& [other_id, other] : customers_by_id) {
                if (other_id != customer_id && !other.depot) {
                    by_distance.emplace_back(calculate_distance(customer, other), other_id);
                }
            }
            std::ranges::sort(by_distance);
            const size_t take = std::min(ng_neighborhood_size_, by_distance.size());
            for (size_t i = 0; i < take; ++i) {
                neighborhood.insert(by_distance[i].second);
            }
        }
        neighborhoods[customer_id] = std::move(neighborhood);
    }

    // The sink is the depot's twin and constrains nothing.
    neighborhoods[sink_id] = {};

    return neighborhoods;
}

void VRPSubproblemNg::construct_resource_graph(RGraphNg* resource_graph,
                                               const std::map<size_t, double>* dual_by_id) {
    LOG_TRACE(__FUNCTION__, '\n');

    // The three components the non-ng harness has, built identically so every number stays
    // comparable, plus the ng memory in a third type slot.
    presets::add_cost_resource<RealResource>(*resource_graph);
    presets::add_window_resource<RealResource>(*resource_graph, time_window_by_customer_id_);
    presets::add_budget_resource<IntResource>(*resource_graph, instance_.get_capacity());

    // The ng-path relaxation. `forbidden_by_customer_id_[v] = {v}` is the ng-route condition: a
    // label arriving at v is infeasible when v is already in the memory its own half accumulated.
    // With an empty neighborhood the memory never retains anything, so the component is present
    // but inert -- which is what makes an ng-versus-no-ng comparison controlled rather than a
    // comparison of two differently shaped labels.
    forbidden_by_customer_id_.clear();
    for (const auto& [customer_id, neighborhood] : ng_neighborhood_by_customer_id_) {
        forbidden_by_customer_id_[customer_id] = {customer_id};
    }
    presets::add_ng_path_resource<SizeTBitsetResource>(*resource_graph,
                                                       ng_neighborhood_by_customer_id_,
                                                       forbidden_by_customer_id_);

    add_all_nodes_to_graph(resource_graph);
    add_all_arcs_to_graph(resource_graph, dual_by_id);
}

void VRPSubproblemNg::update_resource_graph(RGraphNg* /*resource_graph*/,
                                            const std::map<size_t, double>* dual_by_id) {
    LOG_TRACE(__FUNCTION__, '\n');

    const auto max_arc_id = dual_by_id->rbegin()->first;
    std::vector<double> duals(max_arc_id + 1, 0.0);
    for (const auto& [arc_id, dual_value] : *dual_by_id) {
        duals.at(arc_id) = dual_value;
    }

    graph_.update_reduced_costs(duals);
}

void VRPSubproblemNg::add_all_nodes_to_graph(RGraphNg* resource_graph) {
    const auto& customers_by_id = instance_.get_customers_by_id();
    const size_t sink_id = customers_by_id.size();

    for (const auto& [customer_id, customer] : customers_by_id) {
        resource_graph->add_node(customer_id, customer.depot);
        if (customer.depot) {
            depot_id_ = customer.id;
            resource_graph->add_node(sink_id, false, true);
        }
    }
}

void VRPSubproblemNg::add_all_arcs_to_graph(RGraphNg* resource_graph,
                                            const std::map<size_t, double>* dual_by_id) {
    const auto& customers_by_id = instance_.get_customers_by_id();
    const size_t sink_id = customers_by_id.size();

    size_t arc_id = 0;
    for (const auto& [customer_orig_id, customer_orig] : customers_by_id) {
        for (const auto& [customer_dest_id, customer_dest] : customers_by_id) {
            if (customer_orig_id != customer_dest_id) {
                add_arc_to_graph(resource_graph,
                                 customer_orig_id,
                                 customer_dest_id,
                                 customer_orig,
                                 customer_dest,
                                 dual_by_id,
                                 arc_id);
                arc_id++;
            }
        }

        const auto& sink_customer = customers_by_id.at(depot_id_);
        add_arc_to_graph(resource_graph,
                         customer_orig_id,
                         sink_id,
                         customer_orig,
                         sink_customer,
                         dual_by_id,
                         arc_id);
        arc_id++;
    }
}

void VRPSubproblemNg::add_arc_to_graph(RGraphNg* resource_graph, size_t customer_orig_id,
                                       size_t customer_dest_id, const Customer& customer_orig,
                                       const Customer& customer_dest,
                                       const std::map<size_t, double>* dual_by_id, size_t arc_id) {
    const double distance = calculate_distance(customer_orig, customer_dest);
    double customer_pi = 0;
    if (!customer_orig.depot && dual_by_id != nullptr) {
        customer_pi = dual_by_id->at(customer_orig_id);
    }
    auto reduced_cost = distance - customer_pi;
    if (customer_orig.depot && customer_dest.depot) {
        reduced_cost = std::numeric_limits<double>::infinity();
    }

    const auto time = customer_orig.service_time + distance;
    const auto demand = customer_dest.demand;

    // The ng component's arc value is an EMPTY set, not the origin singleton: step 4's narrowing
    // means NgPathExtensionFunction derives the node it adds from the arc's own endpoints, so
    // anything supplied here is ignored in both directions.
    resource_graph->add_arc<RealResource, RealResource, IntResource, SizeTBitsetResource>(
        std::make_tuple(std::make_tuple(reduced_cost),
                        std::make_tuple(time),
                        std::make_tuple(demand),
                        std::make_tuple(std::set<size_t>{})),
        customer_orig_id,
        customer_dest_id,
        distance,
        {Row(customer_orig_id, 1.0)});
}

double VRPSubproblemNg::calculate_distance(const Customer& customer1, const Customer& customer2) {
    return std::sqrt(std::pow(customer2.pos_x - customer1.pos_x, 2) +
                     std::pow(customer2.pos_y - customer1.pos_y, 2));
}
