// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The bidirectional algorithm: both searches, the join, and the oracles they are checked against.
//
// Separate from test_algorithms.cpp because the backward instantiations of the labelling
// templates make one object too big for MinGW's assembler; see the note at the top of
// test_main.cpp. The setup validation and the benchmark are in
// test_bidirectional_validation.cpp, for the same reason: in a Debug build (-O0) nothing is
// inlined, and together they overflow the assembler's string table even without debug info.

#include <gtest/gtest.h>

#include "test_backward_search.hpp"
#include "test_bidirectional.hpp"
#include "test_join.hpp"
#include "test_join_optimality.hpp"
#include "test_label_frontier.hpp"
