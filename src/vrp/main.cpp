// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include <iostream>

#include "cg/subproblem/boost/boost_subproblem.hpp"
#include "instance.hpp"
#include "instance_reader.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "solution_output.hpp"
#include "vrp.hpp"

constexpr size_t DEFAULT_MAX_NB_SOLUTIONS = 20;

int main(int argc, char* argv[]) {
    Logger::init(LogLevel::Trace);

    LOG_TRACE(__FUNCTION__, '\n');

    std::string instance_name = "R101";
    std::string root_dir = file_parent_dir(__FILE__, 3);
    std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";
    if (argc >= 2) {
        instance_path = argv[1];
    }
    size_t subproblem_max_nb_solutions = DEFAULT_MAX_NB_SOLUTIONS;
    if (argc >= 3) {
        subproblem_max_nb_solutions = std::stoull(argv[2]);
    }
    std::string duals_directory = root_dir + "/instances/duals/" + instance_name + "/";
    if (argc >= 4) {
        duals_directory = std::stoull(argv[3]);
    }

    std::string output_path = "../output/" + instance_name + ".txt";

    LOG_INFO("Instance: ", instance_path, '\n');
    InstanceReader instance_reader(instance_path);

    auto instance = instance_reader.read();

    VRP vrp(instance, duals_directory);

    Timer timer(true);
    auto master_solution = vrp.solve(subproblem_max_nb_solutions, false);
    timer.stop();

    LOG_INFO(std::string(45, '*'), '\n');
    LOG_INFO(SolutionOutput::to_string(instance, master_solution, vrp.get_paths()));
    LOG_INFO("Time: ", timer.elapsed_milliseconds(), '\n');

    return 0;
}
