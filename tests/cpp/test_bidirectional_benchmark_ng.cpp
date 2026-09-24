// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The ng benchmark, in its own translation unit: its three-slot resource pack instantiates the
// engine again, and sharing a TU with the algorithm tests overflows MinGW's assembler under
// coverage instrumentation.

#include "test_bidirectional_benchmark_ng.hpp"

#include <gtest/gtest.h>
