// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>
#include <list>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp::parts {

/// @brief An erase callable that erases nothing: the stand-in the @ref Schedule concept checks
///        @c release_stored_aside with. (The search passes its own, which erases through the
///        retention: @c Search::erase.)
template <typename ResourceType>
struct IgnoreErase {
        void operator()(const LabelIterator<ResourceType>& /*position*/) const {}
};

/// @brief What every schedule provides: the order labels are taken in, the per-node quota
///        (@ref PerNodeQuota), and how it trims itself under memory pressure.
///
/// - @c initialize(context) starts a solve, after @c clear. It sizes what the schedule keeps per
///   node.
/// - @c push(stored) hands it a newly kept label: a start label, or one the search has just
///   extended (@c Search::extend_along).
/// - @c size() counts the labels waiting to be handed out. Labels stored aside are not
///   counted. The solve's loop runs while it is above 0, and the status reads it.
/// - @c prepare_next_phase(context) starts the next phase: it hands back what the cuts stored
///   aside, and starts the quota counts again. It is called between phases
///   (@c Algorithm::prepare_next_run, @c Driven::main_loop), and once more when the solve ends,
///   before the status is read, so that labels stored aside count as waiting.
/// - @c trim(context, limit) is called under memory pressure
///   (@c Algorithm::relieve_memory_pressure). It cuts the labels waiting down to @p limit per
///   node (@ref SweepSchedule, @ref PullSchedule), or to @p limit × the number of nodes in all
///   (@ref FifoSchedule, @ref BestFirstSchedule, @ref DepthFirstSchedule).
/// - @c release_stored_aside(context, erase) is called from the second memory-pressure event on.
///   It erases each label stored aside from its node, through @p erase, and releases it.
/// - @c clear() forgets every label, releasing none (the pool owns them). It is called at each
///   new solve, and at each run of a driven search (@c Search::reset, @c Search::restart).
///
/// A schedule loses no label without a trace. Every label pushed is one of these:
/// - handed out (@c next, or @c current for a pulling schedule);
/// - released because it was found dominated, or evicted (see @ref Admission);
/// - cut through @ref StoredAsideLabels: stored aside, so it waits again after
///   @c prepare_next_phase and the status reads it; or, on a bidirectional side, abandoned and
///   counted (@c Algorithm::has_abandoned_labels);
/// - released by @c release_stored_aside under memory pressure, which the proof records
///   (@c ProofLoss::MemoryPressure);
/// - dropped by a child filter (@ref DepthFirstSchedule), which the proof records through the
///   arc mask (@c ProofLoss::ArcsMasked for a mask set with @c set_arc_mask,
///   @c ProofLoss::HeuristicDriver once a driver makes an arc tabu).
///
/// A schedule releases through the pool and erases only through the callable it is handed.
///
/// @tparam ScheduleType The schedule.
/// @tparam ResourceType The resource type carried by labels.
template <typename ScheduleType, typename ResourceType>
concept Schedule = requires(ScheduleType schedule, const ScheduleType const_schedule,
                            detail::SearchContext<ResourceType>& context,
                            const StoredLabel<ResourceType>& stored, size_t limit) {
    schedule.initialize(context);
    schedule.push(stored);
    { const_schedule.size() } -> std::convertible_to<size_t>;
    schedule.prepare_next_phase(context);
    schedule.trim(context, limit);
    schedule.release_stored_aside(context, IgnoreErase<ResourceType>{});
    schedule.clear();
};

/// @brief A schedule the step takes labels from, one at a time: @c next takes the next label off
///        the schedule and returns it, with its position in its node's container, or a null label
///        once none is left.
///
/// The step then judges that label and extends it along every arc leaving its node; the children
/// are pushed back. The schedule no longer holds the label it returned. (To hand out a label is
/// to take it off the schedule and give it to the step.)
template <typename ScheduleType, typename ResourceType>
concept LabelSchedule =
    Schedule<ScheduleType, ResourceType> &&
    requires(ScheduleType schedule, detail::SearchContext<ResourceType>& context) {
        { schedule.next(context) } -> std::same_as<StoredLabel<ResourceType>>;
    };

/// @brief A schedule that pulls: @c visit_next_node moves to the next node and extends into it
///        every label waiting at its predecessors. Pulling's schedule.
///
/// @c visit_next_node calls `extend(label, arc)` for each label waiting at the tail of each arc
/// entering the node (in the direction's sense: @c Direction::in_arcs, @c Direction::tail). The
/// labels it extends stay where they are, for the other nodes they lead to. @c current is the
/// node's new labels, which the step then judges (@c detail::step_node), taking a label off with
/// @c remove_current.
template <typename ScheduleType, typename ResourceType>
concept PullingSchedule =
    Schedule<ScheduleType, ResourceType> &&
    requires(ScheduleType schedule, detail::SearchContext<ResourceType>& context,
             void (*extend)(Label<ResourceType>*, const Arc<ResourceType>*),
             typename std::list<StoredLabel<ResourceType>>::iterator position) {
        schedule.visit_next_node(context, extend);
        schedule.current();
        {
            schedule.remove_current(position)
        } -> std::same_as<typename std::list<StoredLabel<ResourceType>>::iterator>;
    };

/// @brief A schedule that places a label's children: once the step has extended a label along
///        every arc, @c place_children(context, erase) orders the labels pushed since (the label's
///        children) among the labels waiting, and erases (through @p erase) and releases those a
///        child filter drops. A depth-first schedule places children; the others order each
///        label as it is pushed.
template <typename ScheduleType, typename ResourceType>
concept PlacesChildren =
    requires(ScheduleType schedule, detail::SearchContext<ResourceType>& context) {
        schedule.place_children(context, IgnoreErase<ResourceType>{});
    };  // NOLINT(whitespace/newline)

/// @brief Whether @p ScheduleType's order reads the search's completion bound: a schedule says so
///        with `static constexpr bool reads_completion_bound = true`; one that does not say reads
///        none.
template <typename ScheduleType>
inline constexpr bool reads_completion_bound_v = [] {
    if constexpr (requires { ScheduleType::reads_completion_bound; }) {
        return ScheduleType::reads_completion_bound;
    } else {
        return false;
    }
}();

}  // namespace rcspp::parts
