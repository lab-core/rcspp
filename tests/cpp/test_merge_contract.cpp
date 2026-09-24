// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The merge-rule contract tests, in their own translation unit: they instantiate a two-slot
// `<RealResource, SizeTBitsetResource>` pack that nothing else in the suite needs. The ng
// forward-semantics tests share its three-slot pack.

#include "test_merge_contract.hpp"

#include <gtest/gtest.h>

#include "test_ng_forward.hpp"
