// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The ng-path equivalence tests, in their own translation unit.
//
// See the note at the top of test_main.cpp for why the suite is split, and the one at the top of
// test_equivalence_ng.hpp for why *this* header in particular needs its own TU: it is the only
// place a two-slot resource pack is instantiated, which doubles the engine's template
// instantiations and overflowed the assembler's string table when it shared a TU.

#include "test_equivalence_ng.hpp"

#include <gtest/gtest.h>
