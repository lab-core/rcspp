// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include <iostream>

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

    Timer timer(true);
    auto master_solution = vrp.solve(subproblem_max_nb_solutions, false);
    timer.stop();

    SolutionOutput::print(instance, master_solution, vrp.get_paths());

    std::cout << "Time: " << timer.elapsed_milliseconds();

    return 0;
}
