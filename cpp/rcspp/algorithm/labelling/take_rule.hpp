// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief What the step does with a label it has taken from its schedule, as the take rule
///        decides (@ref TakeRule). @ref Search::apply carries out every verdict but @c Extend.
///
/// To erase a label is to take its pointer out of its node's container. To release it is to give
/// it back to the pool through the reference counts (@c LabelPool::release_with_ref_count): it is
/// freed at once, or later, once no label extended from it is alive.
enum class Verdict : std::uint8_t {
    /// Extend it along every arc its direction walks.
    Extend,
    /// Do not extend it, and leave it stored at its node: a label at an end node (its path is
    /// read from there at the end of the solve), or, on a bidirectional side, a label past the
    /// half-way point (the join pairs it).
    Keep,
    /// Release it, without erasing it: a dominated label, which the label that dominated it has
    /// already erased from its node's container.
    Release,
    /// Erase it from its node's container, then release it: a label pruned by the incumbent, or
    /// one of infinite cost.
    EraseAndRelease,
};

/// @brief The take rule's answer for one label.
struct Decision {
        Verdict verdict = Verdict::Extend;
        bool record = false;  ///< an end-node label to record now
};

/// @brief The checks a label goes through when the step takes it from its schedule, written
///        once for every search. @ref step calls both halves, in this order.
///
/// @ref live holds the checks whose answer can change while the label waits in the schedule: the
/// label may have been dominated since it was queued, and the incumbent may have fallen to its
/// cost or below. @ref fixed holds the checks whose answer is fixed when the label is created:
/// whether it is at an end node, whether its cost is infinite, and whether it is past the
/// half-way point. The step runs @ref fixed only when @ref live answers @c Extend, so an end-node
/// label that @ref live releases or prunes is never recorded.
///
/// A label schedule (FIFO) already releases a dominated label in its @c next(); the check here is
/// for the schedules that do not. The per-node quota is not checked here: every schedule applies
/// it itself.
///
/// @tparam ResourceType The resource type carried by labels.
/// @tparam Direction    The direction the label was extended in.
template <typename ResourceType, typename Direction>
struct TakeRule {
        /// @brief The checks whose answer can change while the label waits.
        ///
        /// A dominated label is released (@c Verdict::Release). With @c prune_by_incumbent, a label
        /// that cannot lead to a path cheaper than the incumbent is erased and released
        /// (@c Verdict::EraseAndRelease): its cost plus its completion bound is at or above the
        /// incumbent. That never loses the cheapest path, with negative (reduced) costs too: the
        /// bound is a lower bound on the cost still to pay. Under a cost that is not additive there
        /// is no valid bound (@c SearchContext::prune_adds_completion_bound is false): the prune
        /// compares the cost alone, which may drop the cheapest path when an arc lowers the cost.
        ///
        /// @return Extend when the label passes; otherwise what to do with it.
        template <typename SearchType>
        static Decision live(const Label<ResourceType>& label, const SearchType& /*search*/,
                             const SearchContext<ResourceType>& context) {
            if (label.dominated) {
                return {.verdict = Verdict::Release};
            }
            if (context.params->prune_by_incumbent &&
                label.get_cost() + (context.prune_adds_completion_bound
                                        ? (*context.completion_bound)(label)
                                        : 0.0) >=
                    *context.incumbent) {
                return {.verdict = Verdict::EraseAndRelease};
            }
            return {};
        }

        /// @brief The checks whose answer is fixed when the label is created, run once @ref live
        ///        has answered @c Extend.
        ///
        /// In this order:
        /// 1. A label at an end node stays stored (@c Verdict::Keep), and lowers the incumbent
        ///    when it costs less than the upper bound. It is to be recorded now
        ///    (@c Decision::record) in three cases; otherwise its path is read at the end of the
        ///    solve, from the labels left at the end nodes:
        ///    - the retention records at take (@c records_at_take): it keeps nothing at the end
        ///      nodes;
        ///    - @c return_dominated_solutions is set: a later label may dominate this one and
        ///      erase it from its node before the end, so only a record made now returns it. A
        ///      label dominated before it is taken is released by @ref live, and is never
        ///      recorded;
        ///    - a run that ends at its first path (@c SearchContext::run_ends_at_end_node),
        ///      whatever the retention: the label also stays stored at its node, and
        ///      @c record_path drops it as a duplicate when the run ends.
        ///    The caller records it, then asks @ref SearchContext::should_end_run whether to stop.
        /// 2. A label of infinite cost is erased and released.
        /// 3. With @c prune_by_threshold, a label whose cost plus its completion bound reaches the
        ///    upper bound is erased and released (@ref is_out_of_reach).
        /// 4. On a bidirectional side, a label past the half-way point stays stored, unextended
        ///    (@c Verdict::Keep), for the join to pair.
        ///
        /// @return Extend when the label passes; otherwise what to do with it.
        template <typename SearchType>
        static Decision fixed(const Label<ResourceType>& label, const SearchType& /*search*/,
                              const SearchContext<ResourceType>& context) {
            if (Direction::is_end_node(label.get_end_node())) {
                if (label.get_cost() < context.upper_bound) {
                    *context.incumbent = std::min(*context.incumbent, label.get_cost());
                }
                return {.verdict = Verdict::Keep,
                        .record = SearchType::retention_type::records_at_take ||
                                  context.params->return_dominated_solutions ||
                                  context.run_ends_at_end_node};
            }
            if (std::isinf(label.get_cost())) {
                return {.verdict = Verdict::EraseAndRelease};
            }
            if (context.params->prune_by_threshold && is_out_of_reach(label, context)) {
                return {.verdict = Verdict::EraseAndRelease};
            }
            if (context.half_way != nullptr &&
                context.half_way->template is_past<Direction>(label)) {
                return {.verdict = Verdict::Keep};
            }
            return {};
        }

        /// @brief Whether no path through @p label can cost less than the upper bound: its cost
        ///        plus its completion bound is at or above it, with a margin of the params'
        ///        tolerance, relative to the upper bound when that is above 1 in magnitude.
        ///
        /// Dropping such a label loses no path below the upper bound, under an additive cost. The
        /// bound is consistent: at each arc, the bound at the tail is at most the arc's share plus
        /// the bound at the head. So every label extended from it would be out of reach too. And
        /// every label it evicted at admission cost at least as much at the same node, so it was
        /// out of reach too. A bound of -infinity (a negative-cost cycle) puts nothing out of
        /// reach; one of +infinity (no end node reached) puts the label out of reach of any upper
        /// bound.
        [[nodiscard]] static bool is_out_of_reach(const Label<ResourceType>& label,
                                                  const SearchContext<ResourceType>& context) {
            const double margin =
                context.params->tolerance * std::max(1.0, std::abs(context.upper_bound));
            return label.get_cost() + (*context.completion_bound)(label) >=
                   context.upper_bound + margin;
        }
};

}  // namespace rcspp::detail
