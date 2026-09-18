// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The forward labelling algorithms and the VRP subproblem.
//
// One of several translation units; see the note at the top of test_main.cpp for why the suite is
// split and where a new test header belongs. The bidirectional family moved to
// test_bidirectional_algorithms.cpp once the two together stopped assembling under coverage.

#include <gtest/gtest.h>

#include "test_dive_algorithms.hpp"
#include "test_dominance_algorithms.hpp"
#include "test_equivalence.hpp"
#include "test_label_buckets.hpp"
#include "test_rcspp.hpp"
#include "test_typed_add_resource.hpp"
