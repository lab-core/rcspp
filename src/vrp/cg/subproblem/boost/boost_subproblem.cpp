// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "boost_subproblem.hpp"

#include <boost/config.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/r_c_shortest_paths.hpp>
#include <iostream>
#include <limits>
#include <ranges>

#include "rcspp/algorithm/solution.hpp"

using namespace rcspp;

BoostSubproblem::BoostSubproblem(Instance instance, const std::map<size_t, double>* dual_by_id)
    : source_id_(0),
      sink_id_(0),
      instance_(std::move(instance)),
      dual_by_id_(dual_by_id),
      graph_boost_(construct_boost_graph()) {}

std::vector<Solution> BoostSubproblem::solve() {
    std::vector<Solution> solutions_vector;

    boost::graph_traits<GraphVRPTW>::vertex_descriptor source_vertex = source_id_;
    boost::graph_traits<GraphVRPTW>::vertex_descriptor sink_vertex = sink_id_;

    // std::cout << "source_vertex=" << source_vertex << std::endl;
    // std::cout << "sink_vertex=" << sink_vertex << std::endl;

    std::vector<std::vector<boost::graph_traits<GraphVRPTW>::edge_descriptor>> opt_solutions;
    std::vector<ResourceContainerVRPTW> pareto_opt_resource_containers;

    // std::cout << "r_c_shortest_paths\n";
    boost::r_c_shortest_paths(
        graph_boost_,
        get(&VertexPropertiesVRPTW::id, graph_boost_),
        get(&EdgePropertiesVRPTW::id, graph_boost_),
        source_vertex,
        sink_vertex,
        opt_solutions,
        pareto_opt_resource_containers,
        ResourceContainerVRPTW(0, 0, 0),
        ResourceExtensionFunctionVRPTW(),
        DominanceFunctionVRPTW(),
        std::allocator<boost::r_c_shortest_paths_label<GraphVRPTW, ResourceContainerVRPTW>>(),
        boost::default_r_c_shortest_paths_visitor());

    std::cout << "r_c_shortest_paths: SOLVED!\n";

    std::cout << "SPP with time windows:" << std::endl;
    std::cout << "Number of optimal solutions: ";
    std::cout << static_cast<int>(opt_solutions.size()) << std::endl;

    Solution best_solution;
    double best_cost = std::numeric_limits<double>::infinity();

    for (size_t opt_sol_id = 0; opt_sol_id < opt_solutions.size(); ++opt_sol_id) {
        Solution new_solution;

        // Use reverse iterators to traverse the path in reverse order
        for (const auto& edge_desc : opt_solutions[opt_sol_id] | std::views::reverse) {
            auto orig_node_id = boost::source(edge_desc, graph_boost_);
            auto dest_node_id = boost::target(edge_desc, graph_boost_);
            auto edge = boost::edge(orig_node_id, dest_node_id, graph_boost_);

            auto edge_id_map = get(&EdgePropertiesVRPTW::id, graph_boost_);
            auto edge_id = boost::get(edge_id_map, edge.first);

            new_solution.path_node_ids.push_back(orig_node_id);
            new_solution.path_arc_ids.push_back(edge_id);
        }
        new_solution.path_node_ids.push_back(sink_vertex);

        new_solution.cost = pareto_opt_resource_containers[opt_sol_id].cost;

        if (new_solution.cost < best_cost) {
            best_cost = new_solution.cost;
            best_solution = new_solution;
        }
    }

    solutions_vector.push_back(best_solution);

    bool b_is_a_path_at_all = false;
    bool b_feasible = false;
    bool b_correctly_extended = false;
    ResourceContainerVRPTW actual_final_resource_levels(0, 0);
    boost::graph_traits<GraphVRPTW>::edge_descriptor ed_last_extended_arc;
    boost::check_r_c_path(graph_boost_,
                          opt_solutions[0],
                          ResourceContainerVRPTW(0, 0),
                          true,
                          pareto_opt_resource_containers[0],
                          actual_final_resource_levels,
                          ResourceExtensionFunctionVRPTW(),
                          b_is_a_path_at_all,
                          b_feasible,
                          b_correctly_extended,
                          ed_last_extended_arc);

    return solutions_vector;
}

GraphVRPTW BoostSubproblem::construct_boost_graph() {
    // std::cout << __FUNCTION__ << std::endl;

    GraphVRPTW graph_boost;

    add_vertices(graph_boost);

    add_edges(graph_boost);

    return graph_boost;
}

void BoostSubproblem::add_vertices(GraphVRPTW& graph_boost) {
    // std::cout << __FUNCTION__ << std::endl;

    /*std::cout << "instance_.get_customers_by_id().size()="
      << instance_.get_customers_by_id().size() << std::endl;*/

    for (const auto& [customer_id, customer] : instance_.get_customers_by_id()) {
        if (customer.depot) {
            source_id_ = customer.id;
        }

        /*std::cout << "customer_id: " << customer_id << " | customer.ready_time="
          << customer.ready_time
          << " | customer.due_time=" << customer.due_time
          << " | instance_.get_capacity()=" << instance_.get_capacity() <<
          std::endl;*/
        boost::add_vertex(VertexPropertiesVRPTW(customer_id,
                                                customer.ready_time,
                                                customer.due_time,
                                                instance_.get_capacity()),
                          graph_boost);
    }

    // Add sink vertex
    sink_id_ = instance_.get_customers_by_id().size();
    boost::add_vertex(VertexPropertiesVRPTW(sink_id_,
                                            0,
                                            std::numeric_limits<double>::infinity(),
                                            instance_.get_capacity()),
                      graph_boost);

    /*std::cout << "customer_id: " << sink_id_ << " | sink_customer.ready_time="
      << sink_customer.ready_time
      << " | sink_customer.due_time=" << sink_customer.due_time
      << " | instance_.get_capacity()=" << instance_.get_capacity() <<
      std::endl;*/
}

void BoostSubproblem::add_edges(GraphVRPTW& graph_boost) const {
    // std::cout << __FUNCTION__ << std::endl;

    const auto& customers_by_id = instance_.get_customers_by_id();

    Customer sink_customer = customers_by_id.at(source_id_);
    sink_customer.id = customers_by_id.size();

    size_t arc_id = 0;
    for (const auto& [customer_orig_id, customer_orig] : customers_by_id) {
        for (const auto& [customer_dest_id, customer_dest] : customers_by_id) {
            if (customer_orig_id != customer_dest_id) {
                add_single_edge(arc_id, customer_orig, customer_dest, graph_boost);
                arc_id++;
            }
        }

        add_single_edge(arc_id, customer_orig, sink_customer, graph_boost);
        arc_id++;
    }
}

void BoostSubproblem::add_single_edge(size_t arc_id, const Customer& customer_orig,
                                      const Customer& customer_dest,
                                      GraphVRPTW& graph_boost) const {
    auto distance = calculate_distance(customer_orig, customer_dest);
    double customer_pi = 0;
    if (!customer_orig.depot && dual_by_id_ != nullptr) {
        customer_pi = dual_by_id_->at(customer_orig.id);
    }
    auto reduced_cost = distance - customer_pi;
    if (customer_orig.depot && customer_dest.depot) {
        reduced_cost = std::numeric_limits<double>::infinity();
    }

    auto time = customer_orig.service_time + distance;

    auto demand = customer_dest.demand;

    boost::add_edge(customer_orig.id,
                    customer_dest.id,
                    EdgePropertiesVRPTW(arc_id, reduced_cost, time, demand),
                    graph_boost);

    /*std::cout << "arc_id: " << arc_id << " | " << customer_orig.id << " -> " <<
      customer_dest.id
      << " | cost=" << reduced_cost << " | time=" << time << " | demand=" <<
      demand << std::endl;*/
}

double BoostSubproblem::calculate_distance(const Customer& customer1, const Customer& customer2) {
    auto distance = std::sqrt(std::pow(customer2.pos_x - customer1.pos_x, 2) +
                              std::pow(customer2.pos_y - customer1.pos_y, 2));
    return distance;
}
