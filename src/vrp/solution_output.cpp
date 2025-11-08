// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "solution_output.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include "rcspp/rcspp.hpp"

SolutionOutput::SolutionOutput(std::string duals_directory)
    : duals_directory_(std::move(duals_directory)) {}

std::string SolutionOutput::to_string(const Instance& instance, const MPSolution& solution,
                                      std::vector<Path> paths) {
    const auto& customers_by_id = instance.get_customers_by_id();
    std::ostringstream ss;
    ss << "solution.cost=" << solution.cost << std::endl;
    ss << "solution.paths:" << std::endl;
    for (const auto& [path_id, value] : solution.value_by_var_id) {
        if (value > 0) {
            ss << path_id << ": " << value << std::endl;
            const auto& path = paths.at(path_id);
            ss << path.cost << std::endl;
            // size_t previous_node_id = 0;
            for (auto node_id : path.visited_nodes) {
                if (node_id == customers_by_id.size()) {
                    node_id = 0;
                }

                /*const auto& previous_customer = customers_by_id.at(previous_node_id);
                const auto& customer = customers_by_id.at(node_id);
                ss << node_id << " (" << calculate_distance(previous_customer,
                customer) << ") " << " -> "; previous_node_id = node_id;*/

                ss << node_id << " -> ";
                // previous_node_id = node_id;
            }
            ss << std::endl;
        }
    }
    ss << std::endl;
    return ss.str();
}

void SolutionOutput::save_dual_to_file(const MPSolution& solution, const std::string output_path) {
    std::string full_output_path = duals_directory_ + output_path;

    std::ofstream output_file(full_output_path);

    if (output_file.is_open()) {
        for (const auto& [var_id, dual_value] : solution.dual_by_var_id) {
            output_file << var_id << " " << std::fixed
                        << std::setprecision(std::numeric_limits<double>::max_digits10)
                        << dual_value << std::endl;
        }
    } else {
        LOG_WARN("Unable to open the file: ", output_path, '\n');
    }
}
