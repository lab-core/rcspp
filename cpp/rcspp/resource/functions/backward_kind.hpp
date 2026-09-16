// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

/// @brief How a resource's backward extension relates to its forward one.
///
/// Declared per extension function via @c ExtensionFunction::backward_kind(). A bidirectional
/// solve refuses to start on any component still reporting @c Unspecified. Forward-only solves
/// never read it.
///
/// **Why this lives in its own header.** The kind is declared by the *extension* function but is
/// also the fact the *feasibility* function needs in order to pick its merge test: a bound-style
/// backward value (@c Threshold) is compared against the forward value, while an accumulating one
/// (@c Accumulate) has to be added to it. `FeasibilityFunction` therefore stores the kind too --
/// see @c FeasibilityFunction::set_backward_kind -- and neither header should have to include the
/// other to name the enum. One shared vocabulary header is the alternative to a dependency edge
/// that points the wrong way.
enum class BackwardKind {
    Unspecified,  ///< not declared -- a bidirectional solve will refuse to run
    Accumulate,   ///< no bound; extend_back == extend (e.g. cost)
    Threshold,    ///< stores a deadline/ceiling; extend_back inverts extend and clamps
    Mirror,       ///< stores a set seen on its own half; same formula, origin/destination swapped
};

}  // namespace rcspp
