// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/validation/pre_solve_plan.hpp"

namespace rcspp {

/// @brief Exit status returned by Algorithm::solve().
enum class AlgorithmStatus {
    Complete,      ///< All labels processed; result is optimal (given other params).
    Timeout,       ///< Wall-clock timeout reached before completion.
    MaxSolutions,  ///< max_solutions reached.
    Truncated,     ///< max_phases used up with labels unextended, or a dive's budget spent.
    Interrupted,   ///< External should_stop callback returned true.
    MemoryLimit,   ///< RSS memory limit reached.
};

/// @brief Human-readable name for an @ref AlgorithmStatus value.
///
/// Note: only @ref AlgorithmStatus::Complete means the search was exhaustive; every other
/// status indicates the solve was cut short (so e.g. "no solution found" does not prove that
/// none exists).
[[nodiscard]] inline std::string to_string(AlgorithmStatus status) {
    switch (status) {
        case AlgorithmStatus::Complete:
            return "complete";
        case AlgorithmStatus::Timeout:
            return "timeout";
        case AlgorithmStatus::MaxSolutions:
            return "max_solutions";
        case AlgorithmStatus::Truncated:
            return "truncated";
        case AlgorithmStatus::Interrupted:
            return "interrupted";
        case AlgorithmStatus::MemoryLimit:
            return "memory_limit";
    }
    return "unknown";
}

/// @brief What a bidirectional solve reports about its half-way point and its join.
///
/// On @c SolveResult::bidirectional, which only a bidirectional search fills.
struct BidirectionalStats {
        /// @brief Whether the half-way point was on for this solve.
        ///
        /// Without it, a bidirectional solve does more work than a forward search alone. A
        /// half-way point that is off costs speed, not optimality.
        bool half_way_used = false;

        /// @brief Why the half-way point was off for this solve; empty when it was on.
        std::string half_way_off_reason;

        /// @brief How many distinct complete paths only the join pass produced.
        ///
        /// A path the join produced at several nodes counts once; a path that one of the two
        /// searches also reached on its own does not count. Counted as the join records them,
        /// before the solve keeps its @c max_solutions cheapest solutions, so it can exceed the
        /// joined paths returned.
        size_t number_of_joined_paths = 0;
};

/// @brief Return value of Algorithm::solve().
struct SolveResult {
        std::vector<Solution> solutions;
        AlgorithmStatus status = AlgorithmStatus::Complete;

        /// @brief Whether memory pressure trimmed this solve, so the result may not be optimal.
        ///
        /// Status can still be @c AlgorithmStatus::Complete after a trim, so a caller treating
        /// @c Complete as a proof of optimality must check this too. Same value as
        /// @c Algorithm::memory_pressure_was_triggered().
        bool memory_pressure_triggered = false;

        /// @brief The bidirectional search's half-way point and join; empty from every other
        ///        algorithm.
        std::optional<BidirectionalStats> bidirectional;

        /// @brief Human-readable name of the exit status.
        [[nodiscard]] std::string status_string() const { return to_string(status); }
};

}  // namespace rcspp
