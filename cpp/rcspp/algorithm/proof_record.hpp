// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstdint>
#include <string>

namespace rcspp {

/// @brief What made a solve's result inexact although the search may have finished: what its
///        status cannot see.
///
/// The status already reports every cut a search makes (a quota or a trim that held labels back
/// ends @c Truncated, an abandoned label of a bidirectional side too). These are what a finished
/// search can still have lost.
enum class ProofLoss : std::uint8_t {
    None,              ///< nothing: a complete solve is a proof
    ArcsMasked,        ///< the arc mask left arcs out (@c Algorithm::set_arc_mask)
    RelaxedAdmission,  ///< the admission rule is not exact: it may drop a label dominance keeps
    MemoryPressure,    ///< memory pressure trimmed the labels, and may have released some
};

/// @brief The name of @p loss, as @c SolveResult::proof_lost_by reports it: `"arcs_masked"`,
///        `"relaxed_admission"`, `"memory_pressure"`; empty for @c ProofLoss::None.
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
        [[nodiscard]] bool intact() const { return first_ == ProofLoss::None; }

        /// @brief The first loss recorded; @c ProofLoss::None when none was.
        [[nodiscard]] ProofLoss first_loss() const { return first_; }

    private:
        ProofLoss first_ = ProofLoss::None;
};

}  // namespace rcspp
