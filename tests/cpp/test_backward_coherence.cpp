// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// What `add_resource` accepts, and what the compile-time coherence trait flags.
//
// Its own translation unit: it runs both search families over a resource pack nothing else
// instantiates. See the note at the top of test_main.cpp.

#include "test_backward_coherence.hpp"

#include <gtest/gtest.h>
