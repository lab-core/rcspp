// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "rcspp/algorithm/proof_record.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/validation/pre_solve_plan.hpp"

namespace rcspp {

/// @brief Why Algorithm::solve() ended: @c Complete when no label was left to extend,
///        otherwise the stop that left labels unextended.
///
/// When several stops apply, the first in this order is reported: @c Timeout, @c Interrupted,
/// @c MemoryLimit, @c MaxSolutions, @c Truncated. @c Complete alone does not mean the result
/// is optimal: @c SolveResult::proven says whether it is.
enum class AlgorithmStatus {
    /// No label was left to extend: each was extended, or dropped by a rule of the search
    /// (dominance, a prune, the admission rule).
    Complete,
    Timeout,       ///< `timeout_s` passed with labels still to extend.
    MaxSolutions,  ///< `max_solutions` paths were recorded with labels still to extend.
    /// Labels were left unextended for none of the reasons above: `max_iterations` was reached;
    /// the last of `max_phases` phases ended with labels stored aside (by the per-node quota or a
    /// memory-pressure trim); a bidirectional side abandoned labels; or a driver (the tabu
    /// searches) ended its runs with labels still waiting.
    Truncated,
    Interrupted,  ///< The `should_stop` callback returned true with labels still to extend.
    MemoryLimit,  ///< The process's memory (RSS) reached the limit with labels still to extend.
};

/// @brief Human-readable name for an @ref AlgorithmStatus value.
///
/// Only @ref AlgorithmStatus::Complete means the search ran out of labels; every other status
/// means it was cut short (so e.g. "no solution found" does not prove that none exists). Even
/// @c Complete is a proof only when @c SolveResult::proven is true.
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
        /// Status can still be @c AlgorithmStatus::Complete after a trim; @ref proven is then
        /// @c false. Same value as @c Algorithm::memory_pressure_was_triggered().
        bool memory_pressure_triggered = false;

        /// @brief The bidirectional search's half-way point and join; empty from every other
        ///        algorithm.
        std::optional<BidirectionalStats> bidirectional;

        /// @brief Whether the result is a proof: no path better than those returned exists (none
        ///        at all, when none is returned).
        ///
        /// The status is @c AlgorithmStatus::Complete, and nothing a complete search can still
        /// lose was lost: no arc was masked, the admission rule is exact, no memory pressure
        /// (@c ProofLoss). Same value as @c Algorithm::is_optimal() after the solve.
        bool proven = false;

        /// @brief Why the result is not a proof; empty when it is: the status's name when the
        ///        search did not finish (`"truncated"`, `"timeout"`...), otherwise the first
        ///        @c ProofLoss (`"arcs_masked"`, `"relaxed_admission"`, `"memory_pressure"`).
        std::string proof_lost_by;

        /// @brief Human-readable name of the exit status.
        [[nodiscard]] std::string status_string() const { return to_string(status); }
};

}  // namespace rcspp
