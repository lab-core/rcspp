// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The preset tests, in their own translation unit.
//
// See the note at the top of test_main.cpp for why the suite is split. This header earns its own
// TU because it instantiates three distinct resource packs that nothing else needs --
// `ResourceGraph<RealResource>`, `<IntResource>`, `<SizeTBitsetResource>` and
// `<RealResource, IntResource>` -- one per preset it checks against its hand-built twin. Sharing a
// TU with the algorithm tests pushed the coverage build past MinGW's assembler limit
// ("string table overflow ... file too big").

#include <gtest/gtest.h>

#include "test_presets.hpp"
