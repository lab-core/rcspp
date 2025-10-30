#pragma once
#include <map>

#include "vrp_subproblem/vrp_subproblem.hpp"


bool test_rcspp();

bool test_rcspp_non_integer_dual_row_coef();

bool test_vrp_solve(const std::map<size_t, double>& dual_by_id, VRPSubproblem* vrp_subproblem,
                    double optimal_cost);

