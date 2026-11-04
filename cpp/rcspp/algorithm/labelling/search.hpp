// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/completion_bound.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/admission/dominance_admission.hpp"
#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/algorithm/labelling/parts/observer/no_observer.hpp"
#include "rcspp/algorithm/labelling/parts/retention/keep_at_nodes.hpp"
#include "rcspp/algorithm/labelling/parts/retention/retention.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/algorithm/labelling/take_rule.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp::detail {

// ─── How a labelling solve runs ─────────────────────────────────────────────────────────────
//
// A solve (Algorithm::solve) runs one or two searches. A search (Search) is one direction's labels
// and four parts, named by a SearchParts:
//   - the schedule (parts::Schedule) holds the labels waiting to be extended and picks the next;
//   - the admission rule (parts::Admission) says whether a new label is kept at its node;
//   - the retention (parts::Retention) says where kept labels live;
//   - the observer (parts::Observer) is told how each extension ended.
// Every part reads the solve through the search's context (SearchContext): the graph, the params,
// the label pool, the completion bound, the incumbent and the search's role.
//
// A solve, step by step:
//   1. Start: an empty search, its context, its completion bound when something reads it
//      (compute_completion_bound), and one start label per start node.
//   2. The loop: `step` until no label waits or a stop applies (a budget, an interrupt, the memory
//      limit). One step takes a label off the schedule and runs the take rule on it (TakeRule:
//      the live checks, then the fixed ones). If the label passes, the step extends it along
//      every arc leaving its node (Search::extend_along: look-ahead, extension, feasibility,
//      admission; a kept label is stored, queued and reported). A pulling schedule instead has the
//      step visit one node and pull into it (step_node).
//   3. The next phase: if the per-node quota or memory pressure stored labels aside, they are
//      queued again and the loop runs again, up to max_phases phases.
//   4. The solutions: the end-node labels, recorded when they are taken (a dive's retention,
//      KeepWhileWaiting) or read when the search ends (KeepAtNodes).
//
// Who runs the searches:
//   - LabellingAlgorithm runs one search, forward or backward.
//   - BidirectionalLabellingAlgorithm runs two, one per direction, taking the next step from the
//     side that has done less work (parts::Interleave). Each side stops extending at the half-way
//     point, and the join pairs their labels into paths (join).
//   - A dive (Greedy) keeps no labels at the nodes (parts::KeepWhileWaiting) and goes depth first
//     (parts::DepthFirstSchedule).
//
// Words used throughout:
//   end node          where a path ends in the search's direction: a sink going forward, a
//                     source going backward. A start node is the other end.
//   upper bound       the cost the caller asked to beat; only cheaper paths are solutions.
//   incumbent         the cost of the best path found so far in this solve.
//   completion bound  a lower bound on the cost still to pay from a node to an end node (A*'s h).
//   hand out / take   a schedule hands out a label: its next() takes the label off the schedule
//                     and gives it to the step, which takes it.
//   live / fixed      the take rule's checks whose answer can change while a label waits, and
//                     those whose answer is fixed when the label is created.
//   erase / release   erase: take a label out of its node's container; release: give it back to
//                     the pool, through the reference counts (a label pins its predecessor).
//   cut               the per-node quota or a trim under memory pressure takes a non-dominated
//                     label out of the work.
//   store aside       what a single-direction search does with a cut label: keeps it for the
//                     next phase (parts::StoredAsideLabels).
//   abandon           what a bidirectional side does with a cut label: leaves it in its
//                     container, never extended, and counts it.
//   run               one call of main_loop, one turn of Algorithm::solve's loop. A search
//                     that is not driven makes one run per phase; a driven one (Driven)
//                     makes as many as its driver asks, each of up to max_phases phases.
//   phase             one pass over the labels waiting; the next phase starts with the labels
//                     stored aside.
//   role              whether the search runs alone or as a side of a bidirectional search
//                     (Role).
//   half-way point    where a bidirectional side stops: a label past it is kept, not extended.
//   search            one direction's labels and parts (Search). An algorithm runs one search, or
//                     two (its sides).

/// @brief The parts of one @ref Search, as member types: its schedule, admission rule,
///        retention and observer. Every member is a type, so it costs nothing at run time.
///
/// @tparam ScheduleType      The order labels are taken in (@c parts::Schedule).
/// @tparam AdmissionType     Whether a new label is kept (@c parts::Admission).
/// @tparam RetentionTemplate Where kept labels live, given the container type
///                           (@c parts::Retention).
/// @tparam ObserverType      Told of every extension (@c parts::Observer).
template <typename ScheduleType, typename AdmissionType = parts::DominanceAdmission,
          template <typename> class RetentionTemplate = parts::KeepAtNodes,
          typename ObserverType = parts::NoObserver>
struct SearchParts {
        using schedule = ScheduleType;
        using admission = AdmissionType;
        template <typename ContainerType>
        using retention = RetentionTemplate<ContainerType>;
        using observer = ObserverType;
};

/// @brief One direction of one solve: its labels, its parts, its completion bound and the work
///        it has done, and the only code that extends its labels.
///
/// A single-direction search holds one; a bidirectional search holds two, one per direction,
/// and drives both through the same code.
///
/// @tparam ResourceType  The resource type carried by labels.
/// @tparam Direction     @ref ForwardDirection or @ref BackwardDirection.
/// @tparam ContainerType The per-node container, comparing labels in @p Direction.
/// @tparam PartsType    The parts (@ref SearchParts).
template <typename ResourceType, typename Direction, typename ContainerType, typename PartsType>
struct Search {
        static_assert(DirectionPolicy<Direction, ResourceType>,
                      "Direction does not satisfy DirectionPolicy for this ResourceType");

        using resource_type = ResourceType;
        using direction = Direction;
        using schedule_type = typename PartsType::schedule;
        using retention_type = typename PartsType::template retention<ContainerType>;

        retention_type labels;                           ///< where kept labels live
        typename PartsType::admission admission;         ///< whether a new label is kept
        schedule_type schedule;                          ///< the order labels are taken in
        typename PartsType::observer observer;           ///< told of every extension
        CompletionBound<ResourceType> completion_bound;  ///< the completion bound; empty: none
        /// The extensions tried this solve, one per label and arc, masked and unreachable arcs
        /// included; the interleave compares it.
        size_t work = 0;

        /// @brief Empties the search for a new solve: one container per node, each a copy of
        ///        @p prototype (which carries the container's configuration and direction), an
        ///        empty schedule and no work.
        void reset(size_t number_of_nodes, const ContainerType& prototype) {
            labels.reset(number_of_nodes, prototype);
            schedule.clear();
            work = 0;
        }

        /// @brief Forgets every stored and waiting label, releasing none (the pool owns them).
        void clear() {
            labels.clear();
            schedule.clear();
        }

        /// @brief Creates one start label per start node of @p Direction (sources forward, sinks
        ///        backward), in @c Direction::start_nodes order, stores it and queues it.
        ///
        /// Only start labels need starting values; extension overwrites all others.
        void create_start_labels(const Graph<ResourceType>& graph, LabelPool<ResourceType>* pool) {
            for (const size_t start_node_id : Direction::start_nodes(graph)) {
                auto* start_node = graph.get_node(start_node_id);
                auto& label = pool->get_next_label(start_node);
                Direction::set_start(graph, start_node, label);
                schedule.push(
                    {.label = &label, .position = labels.store(start_node->pos(), &label)});
            }
        }

        /// @brief Extends @p label along @p arc.
        ///
        /// The steps, in order; each of the first four can end the extension:
        ///  1. @c Masked: when the arc mask leaves the arc out (a hard arc, @c
        ///  SearchContext::arc_mask),
        ///     the arc is skipped; no label is taken from the pool.
        ///  2. @c Unreachable: when @p label can no longer reach the arc's head (the look-ahead,
        ///     @c Label::is_reachable), the arc is skipped; no label is taken from the pool.
        ///  3. @c Infeasible: a label is taken from the pool and extended along the arc; when it
        ///     fails its feasibility test it goes back to the pool at once.
        ///  4. @c Rejected: the admission rule decides whether to keep the new label at its node
        ///     (it may evict labels there); a rejected label goes back to the pool at once.
        ///  5. @c Accepted: the new label is stored at its node and pushed to the schedule, and
        ///     @p label becomes its predecessor. That pins @p label: it is not freed while the new
        ///     label is alive (@c Label::ref_count).
        /// The observer is told the outcome in every case.
        ///
        /// @param label The label to extend.
        /// @param arc   The arc to extend it along, one @p Direction walks from its node.
        /// @param context   The search's context: the arc mask, the pool and the admission rule.
        /// @return How the extension ended.
        parts::ExtensionOutcome extend_along(Label<ResourceType>* label,
                                             const Arc<ResourceType>* arc,
                                             SearchContext<ResourceType>& context) {
            using parts::ExtensionOutcome;
            if (context.arc_mask != nullptr && context.arc_mask->is_hard(arc->id)) {
                observer.on_extension(*label, *arc, ExtensionOutcome::Masked);
                return ExtensionOutcome::Masked;
            }
            auto* head_node = Direction::head(*arc);

            // Skip the arc when the label can no longer reach the node it lands on. Both
            // directions call the same `is_reachable`, which is correct only when it depends on
            // the node id alone. A resource whose check reads the label's visited set must declare
            // `JoinRule::Unspecified` (as `ReachableFeasibilityFunction` does), so the pre-solve
            // checks refuse a bidirectional search on it.
            if (!label->is_reachable(head_node->id)) {
                observer.on_extension(*label, *arc, ExtensionOutcome::Unreachable);
                return ExtensionOutcome::Unreachable;
            }

            // Sources and sinks stay out of a path's interior: the graph refuses an arc that
            // enters a source or leaves a sink (Graph::add_arc), so no extension lands on a node
            // this direction starts from.
            assert(!Direction::is_start_node(head_node) &&
                   "an arc enters a source or leaves a sink");

            auto& new_label = context.pool->get_next_label(head_node);
            Direction::extend(*label, *arc, &new_label);
            if (!Direction::feasible(new_label)) {
                context.pool->release_label(&new_label);  // never a predecessor: release at once
                observer.on_extension(*label, *arc, ExtensionOutcome::Infeasible);
                return ExtensionOutcome::Infeasible;
            }
            // The rule decides; the node view's members do the evicting.
            auto node_view = labels.node(head_node->pos());
            if (!admission.admit(new_label, node_view, context.admission)) {
                context.pool->release_label(&new_label);
                observer.on_extension(*label, *arc, ExtensionOutcome::Rejected);
                return ExtensionOutcome::Rejected;
            }
            schedule.push(
                {.label = &new_label, .position = labels.store(head_node->pos(), &new_label)});
            new_label.set_prev_label(label);  // pin the predecessor until this label is released
            observer.on_extension(*label, *arc, ExtensionOutcome::Accepted);
            return ExtensionOutcome::Accepted;
        }

        /// @brief Carries out the take rule's verdict for a label the step does not extend:
        ///        nothing for @c Keep, release for @c Release, erase then release for
        ///        @c EraseAndRelease.
        ///
        /// Under a retention that stores nothing (@c parts::releases_taken_labels_v, a dive's), a
        /// @c Keep label is released too. It is an end-node label, and the step has already
        /// recorded it (such a retention records at take).
        ///
        /// @param taken   The label and its position in its node's container.
        /// @param verdict What to do with it.
        /// @param pool    The label pool.
        void apply(const parts::StoredLabel<ResourceType>& taken, Verdict verdict,
                   LabelPool<ResourceType>* pool) {
            switch (verdict) {
                case Verdict::Extend:
                    return;
                case Verdict::Keep:  // at an end node, or past the half-way point: stays stored
                    if constexpr (parts::releases_taken_labels_v<retention_type>) {
                        pool->release_with_ref_count(taken.label);
                    }
                    return;
                case Verdict::EraseAndRelease:
                    labels.erase(taken.position);
                    [[fallthrough]];
                case Verdict::Release:
                    pool->release_with_ref_count(taken.label);
                    return;
            }
        }

        /// @brief Takes a stored label out of its node's container, releasing nothing.
        void erase(const LabelIterator<ResourceType>& position) { labels.erase(position); }

        /// @brief The labels stored at the end nodes of @p Direction (sinks forward, sources
        ///        backward), end node by end node.
        [[nodiscard]] std::list<Label<ResourceType>*> labels_at_end_nodes(
            const Graph<ResourceType>& graph) const {
            std::list<Label<ResourceType>*> at_end_nodes;
            for (const size_t end_node_id : Direction::end_nodes(graph)) {
                const auto& stored = labels.labels_at(graph.get_node(end_node_id)->pos());
                at_end_nodes.insert(at_end_nodes.end(), stored.begin(), stored.end());
            }
            return at_end_nodes;
        }

        /// @brief Logs every node's labels at TRACE level, naming the nodes from @p graph.
        void print(const Graph<ResourceType>& graph) const {
            if (LOG_TRACE_ACTIVE()) {
                LOG_TRACE("All non dominated labels by node:\n");
                const auto& containers = labels.containers();
                for (size_t pos = 0; pos < containers.size(); ++pos) {
                    LOG_TRACE("Node ", graph.get_sorted_nodes().at(pos)->id, ":\n");
                    containers.at(pos).print_labels();
                }
            }
        }
};

}  // namespace rcspp::detail
