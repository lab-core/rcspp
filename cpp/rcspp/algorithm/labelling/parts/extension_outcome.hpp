// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstdint>

namespace rcspp::parts {

/// @brief How one extension of a label along an arc ended.
enum class ExtensionOutcome : std::uint8_t {
    Masked,       ///< the arc mask rules the arc out (a hard arc); no label was drawn
    Unreachable,  ///< the look-ahead ruled the arc out; no label was drawn
    Infeasible,   ///< the extended label failed its feasibility test and was released
    Rejected,     ///< the admission rule rejected the extended label, which was released
    /// the extended label was kept and handed on: extended in turn or, at an end node, read as a
    /// path, when it is taken or once the search ends, as the retention says (a dive's child
    /// filter may still drop it: @c parts::DepthFirstSchedule::place_children)
    Accepted,
};

}  // namespace rcspp::parts
