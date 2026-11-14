// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <cstddef>

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

/// @brief What every schedule provides: the order labels are taken in, the per-node quota, and
///        how it trims itself.
///
/// @c initialize starts a solve; @c push hands it a newly kept label; @c size counts the labels
/// waiting; @c prepare_next_phase gives back what a cut stored aside; @c trim keeps about
/// @c limit labels per node, under memory pressure; @c release_stored_aside releases what a cut
/// stored aside, erasing each through the callable; @c clear forgets every label (the pool owns
/// them).
///
/// A schedule loses no label silently: every label pushed is handed out, released as dominated,
/// or cut through @ref StoredAsideLabels, so that the solve's status sees the cut. A schedule
/// releases through the pool and erases only through the callable it is handed.
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
