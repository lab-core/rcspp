// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The bidirectional algorithm: both searches, the join, and the oracles they are checked against.
//
// Separate from test_algorithms.cpp because the backward instantiations of the labelling
// templates made one object too big for MinGW's assembler; see the note at the top of
// test_main.cpp.

#include <gtest/gtest.h>

#include "test_backward_search.hpp"
#include "test_bidirectional.hpp"
#include "test_bidirectional_benchmark.hpp"
#include "test_bidirectional_validation.hpp"
#include "test_join_optimality.hpp"
#include "test_label_join.hpp"
