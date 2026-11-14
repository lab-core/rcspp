// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_set>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/arc_mask.hpp"
#include "rcspp/algorithm/labelling/completion_bound.hpp"
#include "rcspp/algorithm/labelling/half_way.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::detail {

/// @brief Whether a search runs alone or as one side of a bidirectional search. A cut reads it
///        (@c parts::StoredAsideLabels::store_aside, through @ref SearchContext::abandons).
enum class Role : std::uint8_t {
    /// A single-direction search: a cut stores the label aside for the next phase.
    Single,
    /// One side of a bidirectional search, which runs one phase: a cut abandons the label (it
    /// stays in its container, never extended) and counts it.
    BidirectionalSide,
};

/// @brief What every part of a search reads of its solve. Built at the start of each solve by
///        @c Algorithm::make_context, one per search (two for a bidirectional search).
///
/// The pointers are valid for the solve. Each points to the graph or to a member of the
/// algorithm, except @ref completion_bound (the search's own, @c Search::completion_bound) and @ref
/// arc_mask (the caller's mask, set by @c Algorithm::set_arc_mask, or a driver's, set by
/// @c Driven::use_arc_mask). The take rule lowers @c *incumbent, and a driver may change
/// @ref arc_mask and @ref run_ends_at_end_node between runs.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
struct SearchContext {
        const Graph<ResourceType>* graph = nullptr;
        LabelPool<ResourceType>* pool = nullptr;
        const AlgorithmBaseParams* params = nullptr;
        /// The cost upper bound the solve was given (`cost_upper_bound`): a path costing at or
        /// above it is not returned. It does not change during the solve; +infinity: no bound.
        double upper_bound = std::numeric_limits<double>::infinity();
        /// The incumbent: the cost of the cheapest path found so far, or @ref upper_bound until a
        /// cheaper one is found. The take rule lowers it at each end-node label. The incumbent
        /// prune (@c prune_by_incumbent) reads it.
        double* incumbent = nullptr;
        /// The per-node quota: the most labels a schedule hands out at one node in one phase. It
        /// starts at @c max_extended_labels_per_node (@c kNoLimit: no quota).
        size_t* quota = nullptr;
        const std::unordered_set<Solution>* solutions = nullptr;  ///< those recorded so far
        parts::AdmissionContext admission;                        ///< the rule and what it reads
        const CompletionBound<ResourceType>* completion_bound = nullptr;  ///< the completion bound
        const ArcMask* arc_mask = nullptr;  ///< the hard arcs left out; none: every arc open
        Role role = Role::Single;
        /// Whether the incumbent prune adds the completion bound to a label's cost: the bound is
        /// valid (the cost is additive). Otherwise the prune compares the cost alone.
        bool prune_adds_completion_bound = false;
        /// A bidirectional side: counts the labels its cuts abandon. Null for a single-direction
        /// search, whose cuts store labels aside.
        size_t* number_of_abandoned_labels = nullptr;
        /// A bidirectional side: the half-way point, past which a label is kept unextended
        /// (@ref HalfWay). Null for a single-direction search.
        const HalfWay<ResourceType>* half_way = nullptr;
        /// Set by a driver (@c Driven::end_runs_at_end_node): each run ends once it records an
        /// end-node label, whatever the retention: the take rule records the label when it is
        /// taken.
        bool run_ends_at_end_node = false;

        /// @brief Whether a cut abandons a label instead of storing it aside: on a bidirectional
        ///        side, which runs one phase, so there is no next phase to store it for.
        [[nodiscard]] bool abandons() const { return role == Role::BidirectionalSide; }

        /// @brief Whether the run ends now that an end-node label was recorded: always in a driven
        ///        run that ends at its first path (@ref run_ends_at_end_node), and otherwise once
        ///        the solve holds @c max_solutions solutions.
        [[nodiscard]] bool should_end_run() const {
            return run_ends_at_end_node || solutions->size() >= params->max_solutions;
        }
};

}  // namespace rcspp::detail
