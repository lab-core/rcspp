// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>

namespace rcspp::parts {

/// @brief Which side of a bidirectional search works next.
///
/// @c should_step_forward is handed both sides; each has @c schedule.size(), the labels waiting,
/// and
/// @c work, the arcs it has extended. It answers @c true for the forward side.
///
/// @tparam InterleaveType The interleave.
/// @tparam ForwardType    The forward side.
/// @tparam BackwardType   The backward side.
template <typename InterleaveType, typename ForwardType, typename BackwardType>
concept Interleave = requires(const InterleaveType interleave, const ForwardType& forward,
                              const BackwardType& backward) {
    { interleave.should_step_forward(forward, backward) } -> std::convertible_to<bool>;
};

}  // namespace rcspp::parts
