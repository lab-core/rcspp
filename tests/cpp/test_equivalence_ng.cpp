// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The ng-path equivalence tests, in their own translation unit: the two-slot ng pack doubles the
// engine's template instantiations, which overflowed the assembler's string table in a shared TU.

#include "test_equivalence_ng.hpp"

#include <gtest/gtest.h>
