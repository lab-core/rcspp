// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// rcspp.cpp�: d�finit le point d'entr�e de l'application.
//

#include <chrono>
#include <iostream>
#include <memory>

#include "cg/subproblem/boost/boost_subproblem.hpp"
#include "instance.hpp"
#include "instance_reader.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "solution_output.hpp"
#include "vrp.hpp"

////#include "rcspp/resource/base/resource.hpp"
////#include "rcspp/resource/base/expander.hpp"
// #include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/concrete/real_resource.hpp"
// #include "rcspp/resource/base/resource.hpp"
// #include "rcspp/resource/base/expander.hpp"

constexpr size_t DEFAULT_MAX_NB_SOLUTIONS = 20;

int main(int argc, char* argv[]) {
    std::cout << __FUNCTION__ << std::endl;

    std::string instance_name = "R101";

    std::string instance_path = "../../../../instances/" + instance_name + ".txt";
    if (argc >= 2) {
        instance_path = argv[1];
    }
    size_t subproblem_max_nb_solutions = DEFAULT_MAX_NB_SOLUTIONS;
    if (argc >= 3) {
        subproblem_max_nb_solutions = std::stoull(argv[2]);
    }
    std::string duals_directory = "../../../../instances/duals/" + instance_name + "/";
    if (argc >= 4) {
        duals_directory = std::stoull(argv[3]);
    }

    std::string output_path = "../output/" + instance_name + ".txt";

    std::cout << "Instance: " << instance_path << std::endl;
    InstanceReader instance_reader(instance_path);

    auto instance = instance_reader.read();

    VRP vrp(instance, duals_directory);
    Logger::instance().init(LogLevel::Trace);

    auto time_start = std::chrono::high_resolution_clock::now();

    auto master_solution = vrp.solve(subproblem_max_nb_solutions, false);

    auto time_end = std::chrono::high_resolution_clock::now();

    SolutionOutput::print(instance, master_solution, vrp.get_paths());

    std::cout
        << "Time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count();

    return 0;
}
