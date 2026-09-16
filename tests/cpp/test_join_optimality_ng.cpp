// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The ng-path tier of the join-optimality sweep, in its own translation unit.
//
// See the note at the top of test_main.cpp for why the suite is split, and the one at the top of
// test_join_optimality_ng.hpp for why *this* header in particular needs its own TU: it instantiates
// the engine over the two-slot ng pack, and test_algorithms.cpp -- where the single-slot
// test_join_optimality.hpp lives -- already carries two packs.

#include "test_join_optimality_ng.hpp"

#include <gtest/gtest.h>
