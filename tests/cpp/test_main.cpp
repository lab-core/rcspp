// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Graph, label and utility tests.
//
// The suite is split across several translation units because a single TU overflows MinGW's COFF
// string table under coverage instrumentation. Each test_*.hpp is included by exactly one TU:
//
//   test_main.cpp            graph, label, and utility tests
//   test_resources.cpp       resource types and their function objects
//   test_algorithms.cpp      the labelling algorithms and the VRP subproblem
//   test_merge_contract.cpp  the merge-rule contract (uses a second resource pack)

#include <gtest/gtest.h>

#include "test_directional_containers.hpp"
#include "test_graph.hpp"
#include "test_half_way_policy.hpp"
#include "test_label.hpp"
#include "test_preprocessor_coverage.hpp"
#include "test_solution_pool.hpp"
#include "test_tabu_list.hpp"
#include "test_timer.hpp"
