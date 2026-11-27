// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstdint>
#include <string>

namespace rcspp {

/// @brief A reason why a solve is not a proof of optimality even though its status may be
///        @c Complete.
///
/// The status already reports every way a search can leave labels unextended: labels stored
/// aside by the per-node quota or memory pressure and never extended make it @c Truncated, and
/// so does a label abandoned by a side of a bidirectional search. The losses listed here are the
/// ones a search that finished can still have suffered.
enum class ProofLoss : std::uint8_t {
    None,  ///< nothing: a complete solve is a proof
    /// the arc mask set (@c Algorithm::set_arc_mask) is not all open: it leaves hard arcs out, or
    /// has soft ones (a child filter may drop children along them; a search that reads no soft
    /// arc loses the proof all the same)
    ArcsMasked,
    RelaxedAdmission,  ///< the admission rule is not exact: it may drop a label dominance keeps
    MemoryPressure,    ///< memory pressure trimmed the labels, and may have released some
    /// a heuristic changed the search between runs: a tabu search's driver, once it makes an arc
    /// tabu (DiversificationSearch records it too, but is never proven anyway)
    HeuristicDriver,
    /// the incumbent prune compared a label's cost alone (the completion bound is not valid: the
    /// cost is not additive, or an arc adds less than its share): it may have dropped the
    /// cheapest path
    IncumbentPrune,
};

/// @brief The name of @p loss, as @c SolveResult::proof_lost_by reports it: `"arcs_masked"`,
///        `"relaxed_admission"`, `"memory_pressure"`, `"heuristic_driver"`, `"incumbent_prune"`;
///        empty for @c ProofLoss::None.
[[nodiscard]] inline std::string to_string(ProofLoss loss) {
    switch (loss) {
        case ProofLoss::None:
            return "";
        case ProofLoss::ArcsMasked:
            return "arcs_masked";
        case ProofLoss::RelaxedAdmission:
            return "relaxed_admission";
        case ProofLoss::MemoryPressure:
            return "memory_pressure";
        case ProofLoss::HeuristicDriver:
            return "heuristic_driver";
        case ProofLoss::IncumbentPrune:
            return "incumbent_prune";
    }
    return "unknown";
}

/// @brief The first loss of proof in a solve; later ones are not kept.
class ProofRecord {
    public:
        /// @brief Nothing lost, for a new solve.
        void reset() { first_ = ProofLoss::None; }

        /// @brief Records @p loss, unless an earlier one was recorded.
        void record(ProofLoss loss) {
            if (first_ == ProofLoss::None) {
                first_ = loss;
            }
        }

        /// @brief Whether nothing was lost.
        [[nodiscard]] bool is_intact() const { return first_ == ProofLoss::None; }

        /// @brief The first loss recorded; @c ProofLoss::None when none was.
        [[nodiscard]] ProofLoss first_loss() const { return first_; }

    private:
        ProofLoss first_ = ProofLoss::None;
};

}  // namespace rcspp
