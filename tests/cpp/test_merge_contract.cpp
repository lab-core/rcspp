// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The merge-rule contract tests, in their own translation unit.
//
// See the note at the top of test_main.cpp for why the suite is split, and the one at the top of
// test_merge_contract.hpp for why this header in particular needs its own TU: it instantiates a
// two-slot `<RealResource, SizeTBitsetResource>` pack that nothing else in the suite needs, which
// instantiates the header-only engine a second time.

#include "test_merge_contract.hpp"

#include <gtest/gtest.h>
