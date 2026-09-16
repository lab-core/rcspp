// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The ng benchmark, in its own translation unit.
//
// See the note at the top of test_main.cpp for why the suite is split. This one carries the
// THREE-slot resource pack, which instantiates the whole engine a third time; sharing a TU with
// the algorithm tests overflows MinGW's assembler under coverage instrumentation.

#include "test_bidirectional_benchmark_ng.hpp"

#include <gtest/gtest.h>
