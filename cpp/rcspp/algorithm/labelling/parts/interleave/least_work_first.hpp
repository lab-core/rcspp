// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp::parts {

/// @brief Takes from whichever side has extended fewer arcs so far, forward on a tie, and from
///        the other side once one has nothing left.
///
/// The sides share no labels, so the order changes neither side's labels, only what exists when
/// a run stops early: a timeout, `max_iterations` or the memory limit then finds both halves
/// partly built, so the join has pairs to test. With the incumbent-based prune on, the sides also
/// share the incumbent, so the order can change which labels are pruned.
///
/// The number of labels waiting would be a poor measure: the backward side holds a single start
/// label until it starts, so choosing by that number would run the whole forward side first.
struct LeastWorkFirst {
        /// @brief Whether the next unit of work is the forward side's.
        ///
        /// @return @c true when the forward side has labels waiting and the backward side has
        ///         none, or has done at least as much work.
        template <typename ForwardType, typename BackwardType>
        [[nodiscard]] bool should_step_forward(const ForwardType& forward,
                                               const BackwardType& backward) const {
            return forward.schedule.size() > 0 &&
                   (backward.schedule.size() == 0 || forward.work <= backward.work);
        }
};

}  // namespace rcspp::parts
