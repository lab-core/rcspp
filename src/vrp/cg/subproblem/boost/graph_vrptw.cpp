// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "graph_vrptw.hpp"

#include <cmath>
#include <iostream>

constexpr double RESOURCE_CONTAINER_EPSILON = 1e-8;

bool operator==(const ResourceContainerVRPTW& res_cont_lhs,
                const ResourceContainerVRPTW& res_cont_rhs) {
    // std::cout << __FUNCTION__ << std::endl;

    return ((std::abs(res_cont_lhs.cost - res_cont_rhs.cost) < RESOURCE_CONTAINER_EPSILON) &&
            (std::abs(res_cont_lhs.time - res_cont_rhs.time) < RESOURCE_CONTAINER_EPSILON) &&
            (std::abs(res_cont_lhs.demand - res_cont_rhs.demand) < RESOURCE_CONTAINER_EPSILON));
}

bool operator<(const ResourceContainerVRPTW& res_cont_lhs,
               const ResourceContainerVRPTW& res_cont_rhs) {
    // std::cout << __FUNCTION__ << std::endl;

    bool result = true;

    if (res_cont_lhs.cost > res_cont_rhs.cost - RESOURCE_CONTAINER_EPSILON) {
        result = false;
    }
    if (res_cont_lhs.time > res_cont_rhs.time - RESOURCE_CONTAINER_EPSILON) {
        result = false;
    }
    if (res_cont_lhs.demand > res_cont_rhs.demand - RESOURCE_CONTAINER_EPSILON) {
        result = false;
    }

    return result;
}
