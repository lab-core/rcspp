// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

/// @file column_generation.hpp
/// @brief Umbrella header of column generation (rcspp::column_generation, USE_COLUMN_GENERATION).
///
/// Column generation is a library of its own, so rcspp.hpp does not include it. This header pulls
/// in all of it but its master solvers, which have headers and libraries of their own:
/// rcspp/column_generation/highs/highs_master_solver.hpp (rcspp_highs) and
/// rcspp/column_generation/gurobi/gurobi_master_solver.hpp (rcspp_gurobi).
///
/// @code
/// #include "rcspp/column_generation.hpp"
/// @endcode

#pragma once

#include "rcspp/column_generation/column_generation.hpp"
#include "rcspp/column_generation/graph_pricer.hpp"
#include "rcspp/column_generation/master_initialization.hpp"
#include "rcspp/column_generation/master_problem.hpp"
#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/master_solver_self_test.hpp"
#include "rcspp/column_generation/master_solver_support.hpp"
#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/pricing_strategy.hpp"
#include "rcspp/column_generation/statistics.hpp"
#include "rcspp/column_generation/tolerances.hpp"
#include "rcspp/column_generation/types.hpp"
