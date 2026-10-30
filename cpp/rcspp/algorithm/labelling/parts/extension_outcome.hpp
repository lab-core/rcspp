// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstdint>

namespace rcspp::parts {

/// @brief How one extension of a label along an arc ended.
enum class ExtensionOutcome : std::uint8_t {
    Unreachable,  ///< the look-ahead ruled the arc out; no label was drawn
    Infeasible,   ///< the extended label failed its feasibility test and was released
    Rejected,     ///< the admission rule rejected the extended label, which was released
    Accepted,     ///< the extended label was kept and handed on to be extended in turn
};

}  // namespace rcspp::parts
