// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The column generation layer, tested with a scripted backend and scripted pricers (no LP solver,
// no graph), in its own translation unit; see the note at the top of test_main.cpp.

#include <gtest/gtest.h>

#include "test_column_generation_initialization.hpp"
#include "test_column_generation_interfaces.hpp"
#include "test_column_generation_loop.hpp"
#include "test_column_generation_master.hpp"
#include "test_column_generation_tolerances.hpp"
