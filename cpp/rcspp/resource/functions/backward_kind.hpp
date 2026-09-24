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
    Unspecified,     ///< not declared -- a bidirectional solve will refuse to run
    Accumulate,      ///< no bound; extend_back == extend (e.g. cost)
    Threshold,       ///< stores a deadline/ceiling; extend_back inverts extend and clamps
    ArcValue,        ///< container filled from the arc's value; extend_back == extend, since
                     ///< the value is the same object in both directions
    EndpointMirror,  ///< container filled from the arc's endpoints; extend_back swaps origin and
                     ///< destination, so the memory at `v` excludes `v` in both directions
};

/// @brief Whether a kind is one of the two container kinds.
///
/// @param kind The kind to test.
/// @return @c true for @c ArcValue and @c EndpointMirror.
[[nodiscard]] constexpr bool is_container_kind(BackwardKind kind) {
    return kind == BackwardKind::ArcValue || kind == BackwardKind::EndpointMirror;
}

// Backward coherence checks. The extension's `backward_kind()` is the single source of truth;
// `ResourceGraph::add_resource` pushes it into the dominance and feasibility functions.
//
// Compile time, via `backward_coherent_v<Ext, Feas>` (asserted by presets, optional elsewhere):
//   1. the extension declares a kind (not `Unspecified`);
//   2. an `Accumulate` extension is not paired with a `BackSeedEnd::Ceiling` feasibility type.
//
// Setup, in `BidirectionalDominanceAlgorithm::describe_problem` (every model, the complete set):
//   3. as 1, at runtime;
//   4. the feasibility function's `merge_rule()` is not `Unspecified`;
//   5. no `DominanceOrder` merge rule on an `Accumulate` resource (it would accept infeasible
//      splices);
//   6. as 2, at runtime via `seeds_itself_out_of_range()`, which also covers seed ends chosen by
//      a constructor argument;
//   7. a feasibility function that `requires_endpoint_mirror()` is paired with `EndpointMirror`
//      (under `ArcValue` the backward label holds its own node and is always rejected);
//   8. no `DominanceOrder` merge rule on a resource without a scalar value.
// The same pass also refuses a composition dominance function with no backward comparison.

}  // namespace rcspp
