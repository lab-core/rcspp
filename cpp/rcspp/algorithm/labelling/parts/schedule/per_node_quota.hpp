// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace rcspp::parts {

// ─── The helpers the schedules share ────────────────────────────────────────────────────────

/// @brief The labels taken at each node, against the per-node quota.
class PerNodeQuota {
    public:
        /// @brief No label taken yet, at any of @p number_of_nodes nodes.
        void reset(size_t number_of_nodes) { taken_.assign(number_of_nodes, 0); }

        /// @brief No label taken yet, for the next phase.
        void restart() { std::ranges::fill(taken_, 0); }

        /// @brief Whether one more label may be taken at node position @p pos, counting it if
        ///        so.
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
