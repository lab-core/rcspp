// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// The bidirectional algorithm: both searches, the join, and the oracles they are checked against.
//
// Split out of test_algorithms.cpp, which had become the heaviest TU in the suite and stopped
// assembling under coverage instrumentation: MinGW reports "file too big" once one object carries
// enough template instantiations, and `-g0` (CMakeLists) and `-Wa,-mbig-obj` are both already
// spent. That is the same growth the other single-subject TUs were carved off to absorb; see the
// note at the top of test_main.cpp.
//
// The line this splits on is the resource pack each group drives hardest, not the alphabet: the
// headers here instantiate the labelling templates a second time through BackwardDirection and
// LabelList<..., BackwardDirection>, which is what doubles the object.

#include <gtest/gtest.h>

#include "test_backward_search.hpp"
#include "test_bidirectional.hpp"
#include "test_bidirectional_benchmark.hpp"
#include "test_bidirectional_validation.hpp"
#include "test_join_optimality.hpp"
#include "test_label_join.hpp"
