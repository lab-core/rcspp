// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The labelling algorithms and the VRP subproblem.
//
// One of several translation units; see the note at the top of test_main.cpp for why the suite is
// split and where a new test header belongs. This is the heaviest TU: each algorithm template is
// instantiated over the VRP resource composition.

#include <gtest/gtest.h>

#include "test_backward_search.hpp"
#include "test_bidirectional.hpp"
#include "test_dive_algorithms.hpp"
#include "test_dominance_algorithms.hpp"
#include "test_label_buckets.hpp"
#include "test_label_join.hpp"
#include "test_rcspp.hpp"
