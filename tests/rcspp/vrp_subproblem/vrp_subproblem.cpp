// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>

#include "vrp_subproblem.hpp"
#include "rcspp/rcspp.hpp"

VRPSubproblem::VRPSubproblem(Instance instance,
                             const std::map<size_t, double>* row_coefficient_by_id)
    : row_coefficient_by_id_(row_coefficient_by_id),
      instance_(std::move(instance)),
      time_window_by_customer_id_(initialize_time_windows()),
      initial_graph_(construct_resource_graph()) {
    std::cout << "VRPSubproblem::VRPSubproblem\n";
}

std::map<size_t, std::pair<double, double>> VRPSubproblem::initialize_time_windows() {
    std::cout << __FUNCTION__ << std::endl;

    std::map<size_t, std::pair<double, double>> time_window_by_customer_id;

    const auto& customers_by_id = instance_.get_customers_by_id();
    for (const auto& [customer_id, customer] : customers_by_id) {
        time_window_by_customer_id.emplace(
            customer_id,
            std::pair<double, double>{customer.ready_time, customer.due_time});
    }

    const auto& source_customer = customers_by_id.at(0);
    size_t sink_id = customers_by_id.size();
    time_window_by_customer_id.emplace(
        sink_id,
        std::pair<double, double>{0, std::numeric_limits<double>::infinity()});

    size_t arc_id = 0;
    for (const auto& [customer_orig_id, customer_orig] : customers_by_id) {
        for (const auto& [customer_dest_id, customer_dest] : customers_by_id) {
            if (customer_orig_id != customer_dest_id) {
                double min_time = 0;
                double max_time = std::numeric_limits<double>::infinity();
                if (time_window_by_customer_id.contains(customer_dest_id)) {
                    min_time = time_window_by_customer_id.at(customer_dest_id).first;
                    max_time = time_window_by_customer_id.at(customer_dest_id).second;
                }
                min_time_window_by_arc_id_.emplace(arc_id, min_time);
                max_time_window_by_node_id_.emplace(customer_dest_id, max_time);

                arc_id++;
            }
        }

        double min_time = 0;
        double max_time = std::numeric_limits<double>::infinity();
        if (time_window_by_customer_id.contains(sink_id)) {
            min_time = time_window_by_customer_id.at(sink_id).first;
            max_time = time_window_by_customer_id.at(sink_id).second;
        }
        min_time_window_by_arc_id_.emplace(arc_id, min_time);
        max_time_window_by_node_id_.emplace(sink_id, max_time);

        arc_id++;
    }

    return time_window_by_customer_id;
}

ResourceGraph<RealResource> VRPSubproblem::construct_resource_graph(
    const std::map<size_t, double>* dual_by_id) {
    std::cout << __FUNCTION__ << std::endl;

    ResourceGraph<RealResource> resource_graph;

    // Distance (cost)
    resource_graph.add_resource<RealResource>(
        std::make_unique<RealAdditionExpansionFunction>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<RealValueCostFunction>(),
        std::make_unique<RealValueDominanceFunction>());

    // Time
    resource_graph.add_resource<RealResource>(
        std::make_unique<TimeWindowExpansionFunction>(min_time_window_by_arc_id_),
        std::make_unique<TimeWindowFeasibilityFunction>(max_time_window_by_node_id_),
        std::make_unique<RealValueCostFunction>(),
        std::make_unique<RealValueDominanceFunction>());

    // Demand
    resource_graph.add_resource<RealResource>(
        std::make_unique<RealAdditionExpansionFunction>(),
        std::make_unique<MinMaxFeasibilityFunction>(0.0, (double)instance_.get_capacity()),
        std::make_unique<RealValueCostFunction>(),
        std::make_unique<RealValueDominanceFunction>());

    add_all_nodes_to_graph(&resource_graph);

    add_all_arcs_to_graph(&resource_graph, dual_by_id);

    return resource_graph;
}

void VRPSubproblem::update_resource_graph(ResourceGraph<RealResource>* resource_graph,
                                const std::map<size_t, double>* dual_by_id) {
    std::cout << __FUNCTION__ << std::endl;

    const auto max_arc_id = dual_by_id->rbegin()->first;
    std::vector<double> duals(max_arc_id + 1, 0.0);
    for (const auto& [arc_id, dual_value] : *dual_by_id) {
        duals.at(arc_id) = dual_value;
    }

    subproblem_graph_.update_reduced_costs(duals);
}

void VRPSubproblem::add_all_nodes_to_graph(ResourceGraph<RealResource>* resource_graph) {
    std::cout << __FUNCTION__ << std::endl;

    const auto& customers_by_id = instance_.get_customers_by_id();
    size_t sink_id = customers_by_id.size();

    for (const auto& [customer_id, customer] : customers_by_id) {
        auto& node = resource_graph->add_node(customer_id, customer.depot);
        if (customer.depot) {
            depot_id_ = customer.id;

            // Add the depot as a sink as well.
            auto& sink_node = resource_graph->add_node(sink_id, false, true);
        }
    }
}

void VRPSubproblem::add_all_arcs_to_graph(ResourceGraph<RealResource>* resource_graph,
                                const std::map<size_t, double>* dual_by_id) {
    const auto& customers_by_id = instance_.get_customers_by_id();
    size_t sink_id = customers_by_id.size();

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

void VRPSubproblem::add_arc_to_graph(ResourceGraph<RealResource>* resource_graph, size_t customer_orig_id,
                           size_t customer_dest_id, const Customer& customer_orig,
                           const Customer& customer_dest,
                           const std::map<size_t, double>* dual_by_id, size_t arc_id) {
    double distance = calculate_distance(customer_orig, customer_dest);
    double customer_pi = 0;
    if (!customer_orig.depot && dual_by_id != nullptr) {
        customer_pi = dual_by_id->at(customer_orig_id);
    }
    auto reduced_cost = distance - customer_pi;
    if (customer_orig.depot && customer_dest.depot) {
        reduced_cost = std::numeric_limits<double>::infinity();
    }

    auto time = customer_orig.service_time + distance;

    auto demand = customer_dest.demand;

    long double row_coefficient = 1.0;
    if (row_coefficient_by_id_ != nullptr) {
        row_coefficient = row_coefficient_by_id_->at(customer_orig_id);
    }

    /*auto& arc = resource_graph->add_arc<RealResource, RealResource, RealResource>(
        {{reduced_cost}, {time}, {demand}},
        customer_orig_id,
        customer_dest_id,
        arc_id,
        distance,
        {Row(customer_orig_id, row_coefficient)});*/

    auto& arc = resource_graph->add_arc<RealResource, RealResource, RealResource>(
        {reduced_cost, time, demand},
        customer_orig_id,
        customer_dest_id,
        arc_id,
        distance,
        {Row(customer_orig_id, row_coefficient)});
}

double VRPSubproblem::calculate_distance(const Customer& customer1, const Customer& customer2) {
    auto distance = std::sqrt(std::pow(customer2.pos_x - customer1.pos_x, 2) +
                              std::pow(customer2.pos_y - customer1.pos_y, 2));
    return distance;
}

double VRPSubproblem::calculate_solution_cost(const Solution& solution) const {
    double cost = 0.0;

    // TODO(patrick): Figure out how to get the cost of an Expander.
    for (auto arc_id : solution.path_arc_ids) {
        cost += initial_graph_.get_arc(arc_id).cost;
    }

    return cost;
}
