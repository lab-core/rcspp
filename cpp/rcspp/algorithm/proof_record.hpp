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
    None,  ///< nothing was lost: a complete solve is a proof
    /// The arc mask (@c Algorithm::set_arc_mask) has hard or soft arcs. A hard arc is left out;
    /// a child filter may drop the children reached along a soft arc. A mask with soft arcs loses
    /// the proof even for a search that ignores them.
    ArcsMasked,
    /// The admission rule is not exact (@c Relaxed with a component left out, @c KBest with a
    /// limit, or a rule of one's own), so it may drop a label that dominance would keep.
    RelaxedAdmission,
    MemoryPressure,  ///< memory pressure trimmed the labels, and may have released some
    /// A heuristic driver changed the search between runs: a tabu search made an arc tabu.
    /// @c DiversificationSearch records it too, although it is never proven anyway.
    HeuristicDriver,
    /// The incumbent prune compared a label's cost alone, because the completion bound is not
    /// valid (the cost is not additive, or an arc adds less than its share). It may have dropped
    /// the cheapest path.
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
