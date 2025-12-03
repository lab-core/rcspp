// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "vrp.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ranges>

#include "cg/master_problem.hpp"
#include "cg/mp_solution.hpp"
#include "cg/subproblem/boost/boost_subproblem.hpp"
#include "rcspp/rcspp.hpp"
#include "rcspp/resource/concrete/functions/extension/ng-path_extension_function.hpp"

constexpr double MICROSECONDS_PER_SECOND = 1e6;

VRP::VRP(Instance instance)
    : instance_(std::move(instance)),
      time_window_by_customer_id_(initialize_time_windows()),
      ng_neighborhood_customer_id_(initialize_ng_neighborhoods(3)),
      solution_output_(std::nullopt) {
    LOG_TRACE("VRP::VRP\n");
    construct_resource_graph(&graph_);
}

VRP::VRP(Instance instance, std::string duals_directory)
    : instance_(std::move(instance)),
      time_window_by_customer_id_(initialize_time_windows()),
      ng_neighborhood_customer_id_(initialize_ng_neighborhoods(3)),
      solution_output_(SolutionOutput(duals_directory)) {
    LOG_TRACE("VRP::VRP\n");
    construct_resource_graph(&graph_);
}

const std::vector<Path>& VRP::generate_initial_paths() {
    LOG_TRACE(__FUNCTION__, '\n');

    const auto& depot_customer = instance_.get_depot_customer();

    const auto& customers_by_id = instance_.get_customers_by_id();

    for (const auto& customer_id : instance_.get_demand_customers_id()) {
        const auto& customer = customers_by_id.at(customer_id);

        auto path_cost = calculate_distance(depot_customer, customer) +
                         calculate_distance(customer, depot_customer);

        // auto path_time = path_cost + customer.service_time;
        // auto path_demand = customer.demand;

        paths_.emplace_back(path_id_,
                            path_cost,
                            std::list<size_t>{depot_customer.id, customer_id, depot_customer.id});

        ++path_id_;
    }

    return paths_;
}

void VRP::sort_nodes() {
    graph_.sort_nodes();
}

void VRP::sort_nodes_by_connectivity() {
    graph_.sort_nodes_by_connectivity();
}

void VRP::sort_nodes_by_min_tw() {
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

    graph_.sort_nodes([time_window_by_customer_id](const auto& node1, const auto& node2) {
        // if a source, put first
        if (node1->source && !node2->source) {
            return true;
        }
        if (!node1->source && node2->source) {
            return false;
        }
        // if a sink, put last
        if (node1->sink && !node2->sink) {
            return false;
        }
        if (!node1->sink && node2->sink) {
            return true;
        }
        if (std::fabs(time_window_by_customer_id.at(node1->id).first -
                      time_window_by_customer_id.at(node2->id).first) < 1e-3) {  // NOLINT
            return time_window_by_customer_id.at(node1->id).second <=
                   time_window_by_customer_id.at(node2->id).second;
        }
        return time_window_by_customer_id.at(node1->id).first <
               time_window_by_customer_id.at(node2->id).first;
    });
}

void VRP::sort_nodes_by_max_tw() {
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

    graph_.sort_nodes([time_window_by_customer_id](const auto& node1, const auto& node2) {
        // if a source, put first
        if (node1->source && !node2->source) {
            return true;
        }
        if (!node1->source && node2->source) {
            return false;
        }
        // if a sink, put last
        if (node1->sink && !node2->sink) {
            return false;
        }
        if (!node1->sink && node2->sink) {
            return true;
        }
        if (std::fabs(time_window_by_customer_id.at(node1->id).second -
                      time_window_by_customer_id.at(node2->id).second) < 1e-3) {  // NOLINT
            return time_window_by_customer_id.at(node1->id).first <=
                   time_window_by_customer_id.at(node2->id).first;
        }
        return time_window_by_customer_id.at(node1->id).second <
               time_window_by_customer_id.at(node2->id).second;
    });
}

MPSolution VRP::solve(std::optional<size_t> subproblem_max_nb_solutions, bool use_boost,
                      std::optional<std::map<size_t, double>> optimal_dual_by_var_id) {
    LOG_TRACE(__FUNCTION__, '\n');

    MPSolution master_solution;
    generate_initial_paths();
    MasterProblem master_problem(instance_.get_demand_customers_id());
    master_problem.construct_model(paths_);

    double min_reduced_cost = -std::numeric_limits<double>::infinity();
    std::map<size_t, double> final_dual_by_id;
    int nb_iter = 0;
    while (min_reduced_cost < -EPSILON) {
        master_solution = master_problem.solve();

        std::string dual_output_file = "iter_" + std::to_string(nb_iter) + ".txt";

        // if (solution_output_.has_value()) {
        //     solution_output_->save_dual_to_file(master_solution, dual_output_file);
        // }

        const auto dual_by_id =
            calculate_dual(master_solution.dual_by_var_id, optimal_dual_by_var_id, nb_iter);

        std::vector<Solution> solutions_rcspp;
        total_subproblem_time_.start();
        solutions_rcspp = solve_with_rcspp(dual_by_id);
        // solutions_rcspp = solve_with_rcspp<PushingDominanceAlgorithmIterators>(dual_by_id);
        // solutions_rcspp = solve_with_rcspp<PullingDominanceAlgorithmIterators>(dual_by_id);
        total_subproblem_time_.stop();

        // for (auto* node: graph_.get_sorted_nodes()) {
        //     LOG_DEBUG("Node ID: ", node->id, " -> ",
        //     time_window_by_customer_id_.at(node->id).first, ", ",
        //     time_window_by_customer_id_.at(node->id).second, '\n'); for (auto* node2:
        //     graph_.get_sorted_nodes()) {
        //         if (node == node2) break;
        //         if (graph_.is_connected(node->id, node2->id) && !graph_.is_connected(node2->id,
        //         node->id)) {
        //             LOG_DEBUG("Wrong ordering between ", node->id, " and ", node2->id, '\n');
        //         }
        //     }
        // }

        std::vector<Solution> solutions_boost;
        total_subproblem_time_boost_.start();
        solutions_boost = solve_with_boost(dual_by_id);
        total_subproblem_time_boost_.stop();

        LOG_DEBUG("Solution BOOST cost: ", solutions_boost[0].cost, '\n');
        LOG_DEBUG("Solution RCSPP cost: ", solutions_rcspp[0].cost, '\n');

        // RCSPP can be better as it uses int for some resources (e.g., load, time)
        if (abs(solutions_rcspp[0].cost - solutions_boost[0].cost) > COST_COMPARISON_EPSILON) {
            LOG_ERROR("RCSPP solution is different from BOOST:",
                      solutions_rcspp[0].cost,
                      " vs ",
                      solutions_boost[0].cost,
                      "\n");
            // break;
        }

        std::vector<Solution> solutions;
        if (use_boost) {
            solutions = solutions_boost;
        } else {
            solutions = solutions_rcspp;
        }

        if (subproblem_max_nb_solutions != std::nullopt) {
            auto nb_solutions = std::min(subproblem_max_nb_solutions.value(), solutions.size());
            solutions = std::vector<Solution>(solutions.begin(), solutions.begin() + nb_solutions);
        }

        std::vector<Solution> negative_red_cost_solutions;

        min_reduced_cost = std::numeric_limits<double>::infinity();
        for (const auto& sol : solutions) {
            min_reduced_cost = std::min(min_reduced_cost, sol.cost);
            if (sol.cost < -EPSILON) {
                negative_red_cost_solutions.push_back(sol);
            }
        }

        add_paths(&master_problem, negative_red_cost_solutions);

        ++nb_iter;

        if (min_reduced_cost >= -EPSILON) {
            final_dual_by_id = master_solution.dual_by_var_id;
        }

        LOG_DEBUG(std::string(45, '*'), '\n');
        LOG_INFO("nb_iter=",
                 nb_iter,
                 " | obj=",
                 master_solution.cost,
                 " | min_reduced_cost=",
                 std::fixed,
                 std::setprecision(std::numeric_limits<double>::max_digits10),
                 min_reduced_cost,
                 " | paths_generated=",
                 negative_red_cost_solutions.size(),
                 " | EPSILON=",
                 EPSILON,
                 '\n');
        LOG_DEBUG(std::string(45, '*'), '\n');
    }

    // Last solve as a MIP
    master_solution = master_problem.solve(false);
    master_solution.dual_by_var_id = final_dual_by_id;

    return master_solution;
}

const std::vector<Path>& VRP::get_paths() const {
    return paths_;
}

std::vector<Solution> VRP::solve_with_boost(const std::map<size_t, double>& dual_by_id) {
    BoostSubproblem subproblem(instance_, &dual_by_id);

    total_subproblem_solve_time_boost_.start();
    auto solutions = subproblem.solve();

    LOG_DEBUG(__FUNCTION__,
              " Time: ",
              total_subproblem_solve_time_boost_.elapsed_milliseconds(/* only_current = */ true),
              " (ms)\n");

    total_subproblem_solve_time_boost_.stop();

    return solutions;
}

std::map<size_t, std::pair<int, int>> VRP::initialize_time_windows() {
    LOG_TRACE(__FUNCTION__, '\n');

    std::map<size_t, std::pair<int, int>> time_window_by_customer_id;

    const auto& customers_by_id = instance_.get_customers_by_id();
    for (const auto& [customer_id, customer] : customers_by_id) {
        time_window_by_customer_id.emplace(
            customer_id,
            std::pair<int, int>{customer.ready_time, customer.due_time});
    }

    const auto& source_customer = customers_by_id.at(0);
    size_t sink_id = customers_by_id.size();
    time_window_by_customer_id.emplace(
        sink_id,
        std::pair<int, int>{0, std::numeric_limits<int>::max() / 2});  // prevent overflow
    max_time_window_by_node_id_.emplace(0,
                                        std::numeric_limits<int>::max() / 2);  // prevent overflow
    node_set_by_node_id_.emplace(0, std::set<size_t>{0});

    int min_time = 0;
    int max_time = std::numeric_limits<int>::max() / 2;  // prevent overflow
    if (time_window_by_customer_id.contains(sink_id)) {
        min_time = time_window_by_customer_id.at(sink_id).first;
        max_time = time_window_by_customer_id.at(sink_id).second;
    }
    min_time_window_by_node_id_[sink_id] = min_time;
    max_time_window_by_node_id_[sink_id] = max_time;
    node_set_by_node_id_.emplace(sink_id, std::set<size_t>{});

    for (const auto& [customer_id, customer] : customers_by_id) {
        int min_time = 0;
        int max_time = std::numeric_limits<int>::max() / 2;  // prevent overflow
        if (time_window_by_customer_id.contains(customer_id)) {
            min_time = time_window_by_customer_id.at(customer_id).first;
            max_time = time_window_by_customer_id.at(customer_id).second;
        }
        min_time_window_by_node_id_[customer_id] = min_time;
        max_time_window_by_node_id_[customer_id] = max_time;
        node_set_by_node_id_.emplace(customer_id, std::set<size_t>{customer_id});
    }

    return time_window_by_customer_id;
}

std::map<size_t, std::set<size_t>> VRP::initialize_ng_neighborhoods(size_t max_size) {
    std::map<size_t, std::set<size_t>> ng_neighborhood_customer_id;
    const auto& customers_by_id = instance_.get_customers_by_id();
    for (const auto& [customer_orig_id, customer_orig] : customers_by_id) {
        if (customer_orig.depot) {
            ng_neighborhood_customer_id[customer_orig_id] = {};
            continue;
        }
        // Compute distances to all other customers
        std::vector<std::pair<size_t, double>> dist;
        for (const auto& [customer_dest_id, customer_dest] : customers_by_id) {
            if (customer_dest.depot) {
                continue;
            }
            dist.emplace_back(customer_dest_id, calculate_distance(customer_orig, customer_dest));
        }
        // Sort by distance
        std::ranges::sort(dist,
                          [](const auto& p1, const auto& p2) { return p1.second < p2.second; });
        // Select the closest max_size customers
        std::set<size_t> neighborhood;
        for (size_t i = 0; i < std::min(max_size, dist.size()); i++) {
            neighborhood.insert(dist[i].first);
        }
        ng_neighborhood_customer_id[customer_orig_id] = neighborhood;
    }

    // sink
    ng_neighborhood_customer_id[customers_by_id.size()] = {};

    return ng_neighborhood_customer_id;
}

void VRP::construct_resource_graph(RGraph* resource_graph,
                                   const std::map<size_t, double>* dual_by_id) {
    LOG_TRACE(__FUNCTION__, '\n');

    // Distance (cost)
    resource_graph->add_resource<RealResource>(
        std::make_unique<AdditionExtensionFunction<RealResource>>(),
        std::make_unique<TrivialFeasibilityFunction<RealResource>>(),
        std::make_unique<ValueCostFunction<RealResource>>(),
        std::make_unique<ValueDominanceFunction<RealResource>>());

    // Time
    using TimeResource = RealResource;
    resource_graph->add_resource<TimeResource>(
        std::make_unique<TimeWindowExtensionFunction<TimeResource>>(min_time_window_by_node_id_),
        std::make_unique<TimeWindowFeasibilityFunction<TimeResource>>(max_time_window_by_node_id_),
        std::make_unique<ValueCostFunction<TimeResource>>(),
        std::make_unique<ValueDominanceFunction<TimeResource>>());

    // Demand
    using DemandResource = IntResource;
    resource_graph->add_resource<DemandResource>(
        std::make_unique<AdditionExtensionFunction<DemandResource>>(),
        std::make_unique<MinMaxFeasibilityFunction<DemandResource>>(0, instance_.get_capacity()),
        std::make_unique<ValueCostFunction<DemandResource>>(),
        std::make_unique<ValueDominanceFunction<DemandResource>>());

    // // Node
    // using NodeResource = SizeTBitsetResource;
    // resource_graph->add_resource<NodeResource>(
    //     std::make_unique<UnionExtensionFunction<NodeResource>>(),
    //     std::make_unique<IntersectFeasibilityFunction<NodeResource>>(node_set_by_node_id_),
    //     std::make_unique<TrivialCostFunction<NodeResource>>(),
    //     std::make_unique<InclusionDominanceFunction<NodeResource>>());

    // // NG path
    // using NgResource = SizeTBitsetResource;  // SizeTBitsetResource SizeTSetResource
    // resource_graph->add_resource<NgResource>(
    //     std::make_unique<NgPathExtensionFunction<NgResource,
    //     size_t>>(ng_neighborhood_customer_id_),
    //     std::make_unique<IntersectFeasibilityFunction<NgResource, std::set<size_t>>>(
    //         node_set_by_node_id_),
    //     std::make_unique<TrivialCostFunction<NgResource>>(),
    //     std::make_unique<InclusionDominanceFunction<NgResource>>());

    add_all_nodes_to_graph(resource_graph);

    add_all_arcs_to_graph(resource_graph, dual_by_id);
}

void VRP::update_resource_graph(RGraph* resource_graph,
                                const std::map<size_t, double>* dual_by_id) {
    LOG_TRACE(__FUNCTION__, '\n');

    const auto max_arc_id = dual_by_id->rbegin()->first;
    std::vector<double> duals(max_arc_id + 1, 0.0);
    for (const auto& [arc_id, dual_value] : *dual_by_id) {
        duals.at(arc_id) = dual_value;
    }

    graph_.update_reduced_costs(duals);
}

void VRP::add_all_nodes_to_graph(RGraph* resource_graph) {
    LOG_TRACE(__FUNCTION__, '\n');

    const auto& customers_by_id = instance_.get_customers_by_id();
    size_t sink_id = customers_by_id.size();

    for (const auto& [customer_id, customer] : customers_by_id) {
        resource_graph->add_node(customer_id, customer.depot);
        if (customer.depot) {
            depot_id_ = customer.id;

            // Add the depot as a sink as well.
            resource_graph->add_node(sink_id, false, true);
        }
    }
}

void VRP::add_all_arcs_to_graph(RGraph* resource_graph,
                                const std::map<size_t, double>* dual_by_id) {
    const auto& customers_by_id = instance_.get_customers_by_id();
    size_t sink_id = customers_by_id.size();

    size_t arc_id = 0;
    for (const auto& [customer_orig_id, customer_orig] : customers_by_id) {
        for (const auto& [customer_dest_id, customer_dest] : customers_by_id) {
            if (!customer_dest.depot && customer_orig_id != customer_dest_id) {
                add_arc_to_graph(resource_graph,
                                 customer_orig_id,
                                 customer_dest_id,
                                 customer_orig,
                                 customer_dest,
                                 dual_by_id,
                                 arc_id);
                ++arc_id;
            }
        }

        /*if (!customer_orig.depot) {
          const auto& sink_customer = customers_by_id.at(depot_id_);
          add_arc_to_graph(graph, customer_orig_id, sink_id, customer_orig,
        sink_customer, dual_by_id, arc_id); ++arc_id;
        }*/

        const auto& sink_customer = customers_by_id.at(depot_id_);
        add_arc_to_graph(resource_graph,
                         customer_orig_id,
                         sink_id,
                         customer_orig,
                         sink_customer,
                         dual_by_id,
                         arc_id);
        ++arc_id;
    }
}

void VRP::add_arc_to_graph(RGraph* resource_graph, size_t customer_orig_id, size_t customer_dest_id,
                           const Customer& customer_orig, const Customer& customer_dest,
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

    // resource_graph->add_arc<RealResource, RealResource, IntResource, SizeTBitsetResource>(
    //     {reduced_cost, time, demand, std::set<size_t>{customer_orig_id}},
    //     customer_orig_id,
    //     customer_dest_id,
    //     arc_id,
    //     distance,
    //     {Row(customer_orig_id, 1.0)});
    resource_graph->add_arc<RealResource, RealResource, IntResource>({reduced_cost, time, demand},
                                                                     customer_orig_id,
                                                                     customer_dest_id,
                                                                     arc_id,
                                                                     distance,
                                                                     {Row(customer_orig_id, 1.0)});
}

double VRP::calculate_distance(const Customer& customer1, const Customer& customer2) {
    auto distance = std::sqrt(std::pow(customer2.pos_x - customer1.pos_x, 2) +
                              std::pow(customer2.pos_y - customer1.pos_y, 2));
    return distance;
}

void VRP::add_paths(MasterProblem* master_problem, const std::vector<Solution>& solutions) {
    LOG_TRACE("VRP::add_paths: ", solutions.size(), '\n');
    std::vector<Path> new_paths;
    for (const auto& solution : solutions) {
        auto solution_cost = calculate_solution_cost(solution);
        new_paths.emplace_back(path_id_, solution_cost, solution.path_node_ids);
        ++path_id_;
    }
    master_problem->add_columns(new_paths);
    paths_.insert(paths_.end(), new_paths.begin(), new_paths.end());
}

double VRP::calculate_solution_cost(const Solution& solution) const {
    double cost = 0.0;

    // TODO(patrick): Figure out how to get the cost of an Extender.
    for (auto arc_id : solution.path_arc_ids) {
        cost += graph_.get_arc(arc_id)->cost;
    }

    return cost;
}

std::map<size_t, double> VRP::calculate_dual(
    const std::map<size_t, double>& master_dual_by_var_id,
    const std::optional<std::map<size_t, double>>& optimal_dual_by_var_id, int nb_iter,
    double alpha_base, int alpha_max_iter) {
    const double alpha = pow(alpha_base, nb_iter);

    std::map<size_t, double> dual_by_var_id;

    if (nb_iter < alpha_max_iter) {
        for (const auto& [var_id, master_dual] : master_dual_by_var_id) {
            double dual = master_dual;
            if (optimal_dual_by_var_id != std::nullopt) {
                dual = ((1 - alpha) * dual) + (alpha * optimal_dual_by_var_id.value().at(var_id));
            }

            dual_by_var_id.emplace(var_id, dual);
        }
    } else {
        for (const auto& [var_id, master_dual] : master_dual_by_var_id) {
            double dual = master_dual;

            dual_by_var_id.emplace(var_id, dual);
        }
    }

    return dual_by_var_id;
}
