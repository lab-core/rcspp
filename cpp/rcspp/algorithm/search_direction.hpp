// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <string_view>

namespace rcspp {

/// @brief Which way a search runs.
///
/// A backward or bidirectional search reads the model's backward semantics, so the pre-solve
/// checks for that direction must pass first (see @c checks_for).
enum class SearchDirection {
    Forward,        ///< From the sources to the sinks: every algorithm.
    Backward,       ///< From the sinks to the sources.
    Bidirectional,  ///< Both, joined where they meet.
};

/// @brief The direction's name, as messages and Python write it.
///
/// @param direction The direction.
/// @return "forward", "backward" or "bidirectional".
[[nodiscard]] constexpr std::string_view to_string(SearchDirection direction) {
    switch (direction) {
        case SearchDirection::Forward:
            return "forward";
        case SearchDirection::Backward:
            return "backward";
        case SearchDirection::Bidirectional:
            return "bidirectional";
    }
    return "unknown";
}

}  // namespace rcspp
