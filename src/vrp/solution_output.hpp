// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <string>

#include "cg/mp_solution.hpp"
#include "cg/path.hpp"
#include "instance.hpp"

class SolutionOutput {
    public:
        SolutionOutput() = default;

        SolutionOutput(std::string duals_directory);

        static std::string to_string(const Instance& instance, const MPSolution& solution,
                                     std::vector<Path> paths);

        void save_dual_to_file(const MPSolution& solution, std::string output_path);

    private:
        std::string duals_directory_;
};
