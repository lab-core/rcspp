// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/graph/arc.hpp"
#include "rcspp/graph/node.hpp"
#include "rcspp/resource/base/resource_type.hpp"

namespace test_util {

// Tiny stack-allocated fixture for tests that need a real Arc to feed
// ExtensionFunction::create(arc). Owns the origin/destination Node objects so the
// Arc's raw Node* pointers remain valid for the fixture's lifetime.
//
// Non-copyable / non-movable because Arc stores Node* into &origin / &destination.
template <typename ResourceType>
    requires rcspp::ResourceTypeConcept<ResourceType>
struct TestArc {
        rcspp::Node<ResourceType> origin;
        rcspp::Node<ResourceType> destination;
        rcspp::Arc<ResourceType> arc;

        TestArc(size_t origin_id, size_t destination_id, size_t arc_id = 0)
            : origin(origin_id, /*source=*/false, /*sink=*/false),
              destination(destination_id, /*source=*/false, /*sink=*/false),
              arc(arc_id, &origin, &destination) {}

        TestArc(const TestArc&) = delete;
        TestArc& operator=(const TestArc&) = delete;
        TestArc(TestArc&&) = delete;
        TestArc& operator=(TestArc&&) = delete;
};

}  // namespace test_util
