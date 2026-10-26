// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The numbers of the theory pages that only C++ can see, in their own translation unit.
//
// Separate because the bidirectional search's backward instantiations make an object too big for
// MinGW's assembler when grouped with others; see the note at the top of test_main.cpp.

#include "test_theory_docs.hpp"

#include <gtest/gtest.h>
