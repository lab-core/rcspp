// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include "rcspp/algorithm/solution.hpp"
#include "rcspp/algorithm/solve_result.hpp"

namespace rcspp::column_generation {

/// @brief How a pricing call ended.
enum class PricingStatus {
    Optimal,        ///< the search finished: no column better than those returned exists
    SolverLimit,    ///< stopped early (time, label or solution limit)
    Infeasible,     ///< the pricing problem has no path at all
    NotApplicable,  ///< nothing to price in this state, so no improving column either
    Unknown,        ///< anything else
};

/// @brief What the loop asks of one pricing call.
struct PricingRequest {
        /// @brief Return only columns whose @c Solution::cost (reduced cost without the
        ///        convexity dual) is below this value.
        double reduced_cost_threshold = 0.0;
        /// @brief Wall-clock limit in seconds.
        double timeout_s = std::numeric_limits<double>::infinity();
        /// @brief Whether the run reads @c PricingResult::search_details; a pricer may skip
        ///        them otherwise.
        bool search_details = false;
};

/// @brief What one stage of a pricing call did, in @c PricingResult::search_details.
struct PricingStageInfo {
        /// @brief The stage's position in the pricer's stages, from 0.
        size_t stage = 0;
        /// @brief How the stage's search ended.
        AlgorithmStatus status = AlgorithmStatus::Complete;
        /// @brief Columns the stage returned (below the threshold).
        size_t new_columns = 0;
        /// @brief Time of the stage's search, in seconds.
        double seconds = 0.0;
        /// @brief Labels the search extended (@c SolveResult::extended_labels); empty when the
        ///        run does not collect it.
        std::optional<size_t> extended_labels;
        /// @brief A bidirectional search's half-way point and join; empty from another search,
        ///        or when the run does not collect it.
        std::optional<BidirectionalStats> bidirectional;
};

/// @brief What one pricing call returns.
struct PricingResult {
        /// @brief The new columns; @c Solution::column holds their cost and rows.
        std::vector<Solution> columns;
        /// @brief How the call ended.
        PricingStatus status = PricingStatus::Unknown;
        /// @brief A lower bound on the best reduced cost of any of its columns, without the
        ///        convexity dual (-infinity when unknown). It must hold whatever the status: the
        ///        loop builds the Lagrangian bound (@c IterationInfo::lower_bound) from it.
        double min_reduced_cost_bound = -std::numeric_limits<double>::infinity();
        /// @brief Whether the stage that gave this result is exact: only an exact call that is
        ///        @c Optimal or @c Infeasible (no path at all), or a call answering
        ///        @c NotApplicable, proves that no improving column exists.
        bool exact_stage = false;
        /// @brief The stages the call ran, in order, the last one the stage it ended with;
        ///        filled when @c PricingRequest::search_details asks for it, and empty from a
        ///        pricer without stages.
        std::vector<PricingStageInfo> search_details;
};

/// @brief What changed in the pricing problem since the last call.
struct PricingChanges {
        /// @brief Arc costs changed.
        bool costs = false;
        /// @brief Resource bounds changed.
        bool bounds = false;
        /// @brief Nodes or arcs were added or removed.
        bool structure = false;
};

/// @brief Finds columns with a negative reduced cost for one block of the master.
///
/// Usable outside @c ColumnGeneration: duals in, columns and a bound out, e.g. from
/// another solver's pricing callback.
class Pricer {
    public:
        Pricer() = default;
        Pricer(const Pricer&) = delete;
        Pricer& operator=(const Pricer&) = delete;
        Pricer(Pricer&&) = delete;
        Pricer& operator=(Pricer&&) = delete;
        virtual ~Pricer() = default;

        /// @brief Prices the master's duals.
        ///
        /// @param duals   One dual per master row, normalized (@c MasterSolver).
        /// @param request Threshold and time limit.
        /// @return The columns found, the status and a bound.
        virtual PricingResult price(std::span<const double> duals,
                                    const PricingRequest& request) = 0;

        /// @brief Tells the pricer its problem changed, so it can refresh cached state.
        virtual void update(const PricingChanges& /*changes*/) {}

        /// @brief Typical magnitude of the costs its columns are made of (default 1).
        ///
        /// Sets the scale of automatic artificial costs and relative tolerances.
        [[nodiscard]] virtual double cost_scale() const { return 1.0; }
};

}  // namespace rcspp::column_generation
