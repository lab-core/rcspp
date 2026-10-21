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
enum class DirectionalResourceKind {
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
/// @param kind The extension function's directional resource kind.
/// @return @c true when the backward order reverses the forward one.
[[nodiscard]] constexpr bool reverses_back_dominance(DirectionalResourceKind kind) {
    return kind == DirectionalResourceKind::Threshold;
}

// Where the backward declarations are checked. A kind is declared only by the extension's
// `directional_resource_kind()`; `ResourceGraph::add_resource` passes it to the feasibility
// function, and tells the dominance function whether backward dominance reverses.
//  - At compile time, `declares_directional_resource_kind_v<Ext>` requires a declared kind (not
//    `Unspecified`). Every preset asserts this; other code may.
//  - Before every backward or bidirectional solve, `BackwardExtensionCheck` checks how the model
//    reads backward: a declared kind, threshold clamps and starts that match the feasibility
//    function, no negative arc consumption where the backward reading assumes none, a
//    feasibility function that `requires_arc_endpoints()` paired with `ArcEndpoints`, an
//    additive cost, and composition functions that can be read backward.
//  - Before a bidirectional solve, `JoinCheck` also checks the join: a declared `join_rule()`
//    that fits the component; accumulations that really add and start backward at the type
//    default, where the join adds their two halves; and a join test for the composition.
// The class comments of the two checks list every problem they refuse.

}  // namespace rcspp
