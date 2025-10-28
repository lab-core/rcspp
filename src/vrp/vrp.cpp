// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "vrp.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ranges>

#include "cg/master_problem.hpp"
#include "cg/mp_solution.hpp"
#include "cg/subproblem/boost/boost_subproblem.hpp"
#include "rcspp/algorithm/dominance_algorithm_iterators.hpp"
#include "rcspp/graph/row.hpp"
#include "rcspp/resource/composition/functions/cost/component_cost_function.hpp"
#include "rcspp/resource/composition/functions/cost/composition_cost_function.hpp"
#include "rcspp/resource/composition/functions/dominance/component_dominance_function.hpp"
#include "rcspp/resource/composition/functions/dominance/composition_dominance_function.hpp"
#include "rcspp/resource/composition/functions/expansion/composition_expansion_function.hpp"
#include "rcspp/resource/composition/functions/feasibility/composition_feasibility_function.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/concrete/functions/cost/real_value_cost_function.hpp"
#include "rcspp/resource/concrete/functions/dominance/real_value_dominance_function.hpp"
#include "rcspp/resource/concrete/functions/expansion/real_addition_expansion_function.hpp"
#include "rcspp/resource/concrete/functions/expansion/time_window_expansion_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/min_max_feasibility_function.hpp"
#include "rcspp/resource/concrete/functions/feasibility/time_window_feasibility_function.hpp"
#include "rcspp/resource/concrete/real_resource_factory.hpp"
#include "rcspp/resource/functions/feasibility/trivial_feasibility_function.hpp"
#include "rcspp/resource/resource_graph.hpp"

constexpr double MICROSECONDS_PER_SECOND = 1e6;

VRP::VRP(Instance instance)
    : instance_(std::move(instance)),
      path_id_(0),
      time_window_by_customer_id_(initialize_time_windows()),
      initial_graph_(construct_resource_graph()),
      solution_output_(std::nullopt) {
    std::cout << "VRP::VRP\n";
}

VRP::VRP(Instance instance, std::string duals_directory)
    : instance_(std::move(instance)),
      path_id_(0),
      time_window_by_customer_id_(initialize_time_windows()),
      initial_graph_(construct_resource_graph()),
      solution_output_(SolutionOutput(duals_directory)) {
    std::cout << "VRP::VRP\n";
}

const std::vector<Path>& VRP::generate_initial_paths() {
    std::cout << __FUNCTION__ << std::endl;

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
                            std::vector<size_t>{depot_customer.id, customer_id, depot_customer.id});

        path_id_++;
    }

    return paths_;
}

MPSolution VRP::solve(std::optional<size_t> subproblem_max_nb_solutions, bool use_boost,
                      std::optional<std::map<size_t, double>> optimal_dual_by_var_id) {
    std::cout << __FUNCTION__ << std::endl;

    MPSolution master_solution;

    generate_initial_paths();

    double min_reduced_cost = -std::numeric_limits<double>::infinity();

    std::map<size_t, double> final_dual_by_id;

    int nb_iter = 0;
    while (min_reduced_cost < -EPSILON) {
        std::cout << "\n*********************************************\n";
        std::cout << "nb_iter=" << nb_iter << " | min_reduced_cost=" << std::fixed
                  << std::setprecision(std::numeric_limits<double>::max_digits10)
                  << min_reduced_cost;
        std::cout << " | EPSILON=" << EPSILON << std::endl;
        std::cout << "*********************************************\n";

        MasterProblem master_problem(instance_.get_demand_customers_id());

        master_problem.construct_model(paths_);

        master_solution = master_problem.solve(true);

        std::string dual_output_file = "iter_" + std::to_string(nb_iter) + ".txt";

        if (solution_output_.has_value()) {
            solution_output_->save_dual_to_file(master_solution, dual_output_file);
        }        

        const auto dual_by_id =
            calculate_dual(master_solution.dual_by_var_id, optimal_dual_by_var_id, nb_iter);

        std::vector<Solution> solutions_rcspp;
        auto subproblem_time_start = std::chrono::high_resolution_clock::now();
        solutions_rcspp = solve_with_rcspp(dual_by_id);
        auto subproblem_time_end = std::chrono::high_resolution_clock::now();
        total_subproblem_time_ += std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      subproblem_time_end - subproblem_time_start)
                                      .count();

        std::vector<Solution> solutions_boost;
        auto subproblem_time_start_boost = std::chrono::high_resolution_clock::now();
        solutions_boost = solve_with_boost(dual_by_id);
        auto subproblem_time_end_boost = std::chrono::high_resolution_clock::now();
        total_subproblem_time_boost_ += std::chrono::duration_cast<std::chrono::nanoseconds>(
                                            subproblem_time_end_boost - subproblem_time_start_boost)
                                            .count();

        std::cout << "Solution BOOST cost: " << solutions_boost[0].cost << std::endl;
        std::cout << "Solution RCSPP cost: " << solutions_rcspp[0].cost << std::endl;

        if (std::abs(solutions_boost[0].cost - solutions_rcspp[0].cost) > COST_COMPARISON_EPSILON) {
            std::cout << "ERROR!!!!\n";
            break;
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

        /*for (const auto& sol : solutions) {

          std::cout << "sol.cost=" << sol.cost << std::endl;
          std::cout << "sol.label_cost=" << sol.label_cost << std::endl;
          std::cout << "sol.label_time=" << sol.label_time << std::endl;
          std::cout << "sol.label_demand=" << sol.label_demand << std::endl;

          std::cout << "sol.path_node_ids: ";
          for (auto node_id : sol.path_node_ids) {
            std::cout << node_id << " -> ";
          }
          std::cout << std::endl;

          std::cout << "sol.path_arc_ids: ";
          for (auto arc_id : sol.path_arc_ids) {
            std::cout << arc_id << " -> ";
          }
          std::cout << std::endl;

        }*/

        std::vector<Solution> negative_red_cost_solutions;

        min_reduced_cost = std::numeric_limits<double>::infinity();
        for (const auto& sol : solutions) {
            min_reduced_cost = std::min(min_reduced_cost, sol.cost);
            if (sol.cost < -EPSILON) {
                negative_red_cost_solutions.push_back(sol);
            }
        }

        add_paths(negative_red_cost_solutions);

        nb_iter++;

        if (min_reduced_cost >= -EPSILON) {
            final_dual_by_id = master_solution.dual_by_var_id;
        }
    }

    std::cout << "\n*********************************************\n";
    std::cout << "nb_iter=" << nb_iter << " | min_reduced_cost=" << min_reduced_cost
              << " | EPSILON=" << EPSILON << std::endl;
    std::cout << "total_subproblem_time_: " << (total_subproblem_time_ / MICROSECONDS_PER_SECOND)
              << std::endl;
    std::cout << "total_subproblem_solve_time_: "
              << (total_subproblem_solve_time_ / MICROSECONDS_PER_SECOND) << std::endl;
    std::cout << "total_subproblem_time_boost_: "
              << (total_subproblem_time_boost_ / MICROSECONDS_PER_SECOND) << std::endl;
    std::cout << "total_subproblem_solve_time_boost_: "
              << (total_subproblem_solve_time_boost_ / MICROSECONDS_PER_SECOND) << std::endl;
    std::cout << "*********************************************\n";

    // Last solve
    MasterProblem master_problem(instance_.get_demand_customers_id());
    master_problem.construct_model(paths_);
    master_solution = master_problem.solve();

    /*std::cout << "final_dual_by_id:" << final_dual_by_id.size() << std::endl;
    for (const auto& [id, dual_value] : final_dual_by_id) {
      std::cout << id << ": " << dual_value << std::endl;
    }*/

    master_solution.dual_by_var_id = final_dual_by_id;
    /*std::cout << "master_solution.dual_by_var_id:" <<
    master_solution.dual_by_var_id.size() << std::endl; for (const auto& [id,
    dual_value] : master_solution.dual_by_var_id) { std::cout << id << ": " <<
    dual_value << std::endl;
    }*/

    return master_solution;
}

const std::vector<Path>& VRP::get_paths() const {
    return paths_;
}

std::vector<Solution> VRP::solve_with_rcspp(const std::map<size_t, double>& dual_by_id) {
    std::cout << __FUNCTION__ << std::endl;

    if (subproblem_graph_.get_number_of_nodes() == 0) {
        subproblem_graph_ = construct_resource_graph(&dual_by_id);
    } else {
        update_resource_graph(&subproblem_graph_, &dual_by_id);
    }

    auto time_start = std::chrono::high_resolution_clock::now();
    auto solutions = subproblem_graph_.solve();
    // A different algorithm can be specified as a template argument.
    // auto solutions = subproblem_graph_.solve<DominanceAlgorithmIterators>();
    auto time_end = std::chrono::high_resolution_clock::now();

    total_subproblem_solve_time_ +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count();

    std::cout
        << __FUNCTION__ << " Time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count()
        << std::endl;

    return solutions;
}

std::vector<Solution> VRP::solve_with_boost(const std::map<size_t, double>& dual_by_id) {
    BoostSubproblem subproblem(instance_, &dual_by_id);

    auto time_start = std::chrono::high_resolution_clock::now();
    auto solutions = subproblem.solve();
    auto time_end = std::chrono::high_resolution_clock::now();

    total_subproblem_solve_time_boost_ +=
        std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count();

    std::cout
        << __FUNCTION__ << " Time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count()
        << std::endl;

    return solutions;
}

std::map<size_t, std::pair<double, double>> VRP::initialize_time_windows() {
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

ResourceGraph<RealResource> VRP::construct_resource_graph(
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

void VRP::update_resource_graph(ResourceGraph<RealResource>* resource_graph,
                                const std::map<size_t, double>* dual_by_id) {
    std::cout << __FUNCTION__ << std::endl;

    const auto max_arc_id = dual_by_id->rbegin()->first;
    std::vector<double> duals(max_arc_id + 1, 0.0);
    for (const auto& [arc_id, dual_value] : *dual_by_id) {
        duals.at(arc_id) = dual_value;
    }

    subproblem_graph_.update_reduced_costs(duals);
}

void VRP::add_all_nodes_to_graph(ResourceGraph<RealResource>* resource_graph) {
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

void VRP::add_all_arcs_to_graph(ResourceGraph<RealResource>* resource_graph,
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

        /*if (!customer_orig.depot) {
          const auto& sink_customer = customers_by_id.at(depot_id_);
          add_arc_to_graph(graph, customer_orig_id, sink_id, customer_orig,
        sink_customer, dual_by_id, arc_id); arc_id++;
        }*/

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

void VRP::add_arc_to_graph(ResourceGraph<RealResource>* resource_graph, size_t customer_orig_id,
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

    /*auto& arc = resource_graph->add_arc({ { {reduced_cost}, {time},
      {demand} } }, customer_orig_id, customer_dest_id, arc_id, distance,
      {Row(customer_orig_id, 1.0)});*/

    auto& arc = resource_graph->add_arc<RealResource, RealResource, RealResource>(
        {{reduced_cost}, {time}, {demand}},
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

void VRP::add_paths(const std::vector<Solution>& solutions) {
    std::cout << "VRP::add_paths: " << solutions.size() << std::endl;
    for (const auto& solution : solutions) {
        auto solution_cost = calculate_solution_cost(solution);
        paths_.emplace_back(path_id_, solution_cost, solution.path_node_ids);
        path_id_++;
    }
}

double VRP::calculate_solution_cost(const Solution& solution) const {
    double cost = 0.0;

    // TODO(patrick): Figure out how to get the cost of an Expander.
    for (auto arc_id : solution.path_arc_ids) {
        cost += initial_graph_.get_arc(arc_id).cost;
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
