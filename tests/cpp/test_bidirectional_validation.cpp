// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The bidirectional search's setup validation and its benchmark, in their own translation unit:
// with the rest of test_bidirectional_algorithms.cpp they overflow MinGW's assembler string table
// in a Debug build. See the note at the top of test_main.cpp.

#include "test_bidirectional_validation.hpp"

#include <gtest/gtest.h>

#include "test_bidirectional_benchmark.hpp"
