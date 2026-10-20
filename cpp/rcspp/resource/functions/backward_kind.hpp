// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

/// @brief How a resource is read backward, from a sink towards a source.
///
/// A bidirectional solve runs a second search backward and joins its labels with the forward ones,
/// so every resource needs a backward meaning. The kind fixes it: how @c extend_back relates to
/// @c extend, which way backward dominance compares (@ref reverses_back_dominance), and how the
/// join tests two halves. Declared by each extension function; a bidirectional solve refuses a
/// component left @c Unspecified, and forward-only solves never read it.
enum class BackwardKind {
    Unspecified,  ///< no backward meaning declared; a bidirectional solve refuses the resource
    Accumulate,   ///< a sum along the path (e.g. cost); backward sums the suffix, same formula
    Threshold,    ///< a bounded scalar (time, capacity); backward holds the most the prefix
                  ///< may reach at the node, so extend_back inverts extend
    ArcValue,     ///< a set filled from the arc's value, the same in both directions
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

}  // namespace rcspp
