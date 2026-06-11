// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

/// @brief Concept that constrains types usable as a resource value in the RCSPP framework.
///
/// A type satisfies `ResourceTypeConcept` if it exposes the four operations required by the
/// resource management layer: `reset()`, `get_value()`, `set_value()`, and `to_string()`.
///
/// @tparam ResourceType The candidate type to check against this concept.
template <typename ResourceType>
concept ResourceTypeConcept = requires(ResourceType t) {
    t.reset();
    t.get_value();
    t.set_value(t.get_value());
    t.to_string();
};

}  // namespace rcspp
