// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace rcspp::parts {

// ─── The helpers the schedules share ────────────────────────────────────────────────────────

/// @brief How many labels each node has handed out in the current phase, against the per-node
///        quota.
///
/// The quota is @c *context.quota: the params' @c max_extended_labels_per_node (@c kNoLimit means
/// none). A label counts when the schedule hands it out, whatever the take rule then does with
/// it: an end-node label, or one the incumbent prunes, counts too. A label found dominated does
/// not count. The counts start again at each phase (@ref restart).
///
/// The schedules that hand out one label at a time (FIFO, best-first) use this class. The
/// node-by-node schedules apply the quota differently. @ref SweepSchedule cuts a node's queue to
/// it each time it visits the node, so per visit rather than per phase, end nodes included.
/// @ref PullSchedule cuts a node's new labels to it after each pull, except at an end node.
class PerNodeQuota {
    public:
        /// @brief No label taken yet, at any of @p number_of_nodes nodes.
        void reset(size_t number_of_nodes) { taken_.assign(number_of_nodes, 0); }

        /// @brief No label taken yet, for the next phase.
        void restart() { std::ranges::fill(taken_, 0); }

        /// @brief Counts one more label taken at node position @p pos, if the quota allows it.
        ///
        /// @return Whether it did.
        [[nodiscard]] bool try_take(size_t pos, size_t quota) {
            size_t& taken = taken_.at(pos);
            if (taken < quota) {
                ++taken;
                return true;
            }
            return false;
        }

    private:
        std::vector<size_t> taken_;
};

}  // namespace rcspp::parts
