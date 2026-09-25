// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The heuristic study: the column validator, and the correctness sweep every heuristic
// configuration must pass.
//
// A translation unit of its own: the three-slot pricing pack instantiates every algorithm again,
// and sharing a TU with the other algorithm tests would overflow MinGW's assembler (see the note
// at the top of test_main.cpp).

#include <gtest/gtest.h>

#include "test_column_validator.hpp"
#include "test_heuristic_correctness.hpp"
