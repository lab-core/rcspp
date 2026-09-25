// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

// Resource types and their function objects. See test_main.cpp for how the suite is split.

#include <gtest/gtest.h>

#include "resource/concrete/functions/extension/test_backward_kind_declared.hpp"
#include "resource/concrete/functions/extension/test_budget_extension_function.hpp"
#include "resource/concrete/functions/extension/test_ng_path_extension_function.hpp"
#include "resource/concrete/functions/extension/test_time_window_extension_function.hpp"
#include "resource/concrete/functions/feasibility/test_intersection_feasibility_function.hpp"
#include "resource/test_back_seed.hpp"
#include "resource/test_backward_dominance.hpp"
#include "resource/test_endpoint_mirror_form.hpp"
#include "resource/test_merge_rules.hpp"
#include "resource/test_ng_neighborhoods.hpp"
#include "resource/test_node_bounds.hpp"
#include "resource/test_threshold_form.hpp"
#include "test_backward_api.hpp"
#include "test_container_resources.hpp"
#include "test_ng_growth.hpp"
#include "test_resource_base.hpp"
