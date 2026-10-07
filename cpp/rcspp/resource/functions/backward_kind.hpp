// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

/// @brief How a resource is read backward, from a sink towards a source.
///
/// A backward search reads every resource from the sinks, and a bidirectional one also joins its
/// labels with the forward ones, so every resource needs a backward meaning. The kind fixes it: how
/// @c extend_back relates to @c extend, which way backward dominance compares
/// (@ref reverses_back_dominance), and how the join tests two halves. Declared by each extension
/// function; a backward or bidirectional solve refuses a component left @c Unspecified
/// (@c BackwardExtensionCheck), and forward solves never read it.
enum class BackwardKind {
    Unspecified,   ///< no backward meaning declared; a backward or bidirectional solve refuses it
    Accumulate,    ///< a sum along the path (e.g. cost); backward sums the suffix, same formula
    Threshold,     ///< a bounded scalar (time, capacity); backward holds the most the prefix
                   ///< may reach at the node, so extend_back inverts extend
    ArcValue,      ///< a set filled from the arc's value, the same in both directions
    ArcEndpoints,  ///< a set filled from the arc's endpoints; backward swaps origin and destination
};

/// @brief Whether backward dominance under @p kind is the forward comparison, swapped.
///
/// A @c Threshold backward value is a deadline, and a later deadline admits more paths, so the
/// larger value is the better one going backward. Every other kind is read the same way in both
/// directions. @c ResourceGraph::add_resource passes the answer to the dominance function.
///
/// @param kind The extension function's backward kind.
/// @return @c true when the backward order reverses the forward one.
[[nodiscard]] constexpr bool reverses_back_dominance(BackwardKind kind) {
    return kind == BackwardKind::Threshold;
}

// Backward coherence checks. The extension's `backward_kind()` is the single source of truth;
// `ResourceGraph::add_resource` pushes it into the dominance and feasibility functions.
//
// Compile time, via `backward_coherent_v<Ext, Feas>` (asserted by presets, optional elsewhere):
//   1. the extension declares a kind (not `Unspecified`).
//
// At solve time, in the model checks, before any backward or bidirectional search (every model,
// the complete set):
//   2. as 1, at runtime (`BackwardExtensionCheck`);
//   3. the feasibility function's `join_rule()` is not `Unspecified` (`JoinCheck`);
//   4. no `ValueOrder` join rule on an `Accumulate` resource (it would accept infeasible
//      splices) (`JoinCheck`);
//   5. a feasibility function that `requires_arc_endpoints()` is paired with `ArcEndpoints`
//      (under `ArcValue` the backward label holds its own node and is always rejected)
//      (`BackwardExtensionCheck`);
//   6. no `ValueOrder` join rule on a resource without a scalar value (`JoinCheck`).
// `BackwardExtensionCheck` also refuses a composition dominance function with no backward
// comparison.

}  // namespace rcspp
