// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

template <typename ResourceType>
concept ResourceTypeConcept = requires(ResourceType t) {
    t.reset();
    t.get_value();
};

}  // namespace rcspp
