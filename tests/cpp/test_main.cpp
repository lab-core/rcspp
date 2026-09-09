// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Graph, label and utility tests.
//
// The suite is split across several translation units rather than one, because the header-only
// templates instantiate per TU and a single TU exceeds the COFF limits under coverage
// instrumentation: MinGW's assembler aborts with "string table overflow", one .pdata$<mangled-name>
// section per template instantiation against a 10MB string table. Splitting keeps the gcov build
// (and so the local coverage gate) working as the suite grows.
//
// Each test_*.hpp is included by EXACTLY ONE of these TUs; gtest_main supplies main().
//
//   test_main.cpp         graph, label, and utility tests
//   test_resources.cpp    resource types and their four function objects
//   test_algorithms.cpp   the labelling algorithms and the VRP subproblem
//
// A new test header goes in whichever TU matches its subject, alphabetically within that TU.

#include <gtest/gtest.h>

#include "test_directional_containers.hpp"
#include "test_graph.hpp"
#include "test_half_way_policy.hpp"
#include "test_label.hpp"
#include "test_preprocessor_coverage.hpp"
#include "test_solution_pool.hpp"
#include "test_tabu_list.hpp"
#include "test_timer.hpp"
