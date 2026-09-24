// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

/// @brief How a resource's backward extension relates to its forward one.
///
/// Declared per extension function via @c ExtensionFunction::backward_kind(). A bidirectional
/// solve refuses to start on any component still reporting @c Unspecified. Feasibility functions
/// also store it to pick their merge test, hence the shared header.
enum class BackwardKind {
    Unspecified,  ///< not declared -- a bidirectional solve will refuse to run
    Accumulate,   ///< no bound; extend_back == extend (e.g. cost)
    Threshold,    ///< stores a deadline/ceiling; extend_back inverts extend and clamps
    Mirror,       ///< stores a set seen on its own half; same formula, origin/destination swapped
};

}  // namespace rcspp
