// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Resource types and their four function objects.
//
// One of several translation units; see the note at the top of test_main.cpp for why the suite is
// split and where a new test header belongs.

#include <gtest/gtest.h>

#include "resource/concrete/functions/extension/test_backward_kind_declared.hpp"
#include "resource/concrete/functions/extension/test_budget_extension_function.hpp"
#include "resource/concrete/functions/extension/test_ng_path_extension_function.hpp"
#include "resource/concrete/functions/extension/test_time_window_extension_function.hpp"
#include "resource/concrete/functions/feasibility/test_intersection_feasibility_function.hpp"
#include "resource/test_backward_dominance.hpp"
#include "test_backward_api.hpp"
#include "test_container_resources.hpp"
#include "test_resource_base.hpp"
