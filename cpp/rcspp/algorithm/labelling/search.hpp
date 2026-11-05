// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <list>
#include <utility>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/bounds.hpp"
#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/take_rule.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/label/label_pool.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp::detail {

/// @brief A named combination of a search's parts. Every member is a type, so a preset costs
///        nothing at run time.
///
/// @tparam ScheduleType      The order labels are taken in (@c parts::Schedule).
/// @tparam AdmissionType     Whether a new label is kept (@c parts::Admission).
/// @tparam RetentionTemplate Where kept labels live, given the container type
///                           (@c parts::Retention).
/// @tparam ObserverType      Told of every extension (@c parts::Observer).
template <typename ScheduleType, typename AdmissionType = parts::DominanceAdmission,
          template <typename> class RetentionTemplate = parts::KeepAtNodes,
          typename ObserverType = parts::NoObserver>
struct Preset {
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
/// @tparam PresetType    The parts (@ref Preset).
template <typename ResourceType, typename Direction, typename ContainerType, typename PresetType>
struct Search {
        static_assert(DirectionPolicy<Direction, ResourceType>,
                      "Direction does not satisfy DirectionPolicy for this ResourceType");

        using resource_type = ResourceType;
        using direction = Direction;
        using schedule_type = typename PresetType::schedule;
        using retention_type = typename PresetType::template retention<ContainerType>;

        retention_type labels;                     ///< where kept labels live
        typename PresetType::admission admission;  ///< whether a new label is kept
        schedule_type schedule;                    ///< the order labels are taken in
        typename PresetType::observer observer;    ///< told of every extension
        NodeBound<ResourceType> bound;             ///< the completion bound; empty: none
        size_t work = 0;                           ///< arcs extended this solve

        /// @brief Empties the search for a new solve: one container per node, each a copy of
        ///        @p prototype (which carries the container's configuration and direction), an
        ///        empty schedule and no work.
        void reset(size_t num_nodes, const ContainerType& prototype) {
            labels.reset(num_nodes, prototype);
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
        /// In this order: the look-ahead (an unreachable head draws no label); a label is drawn
        /// and extended; an infeasible one is released; the admission rule decides; a kept label
        /// is stored, queued, and pinned to @p label (its predecessor stays alive until it is
        /// released); a rejected one is released at once. The observer is told every outcome.
        ///
        /// @param label The label to extend.
        /// @param arc   The arc to extend it along, one @p Direction walks from its node.
        /// @param ctx   The search's context: the pool and the admission rule.
        /// @return How the extension ended.
        parts::ExtensionOutcome extend_along(Label<ResourceType>* label,
                                             const Arc<ResourceType>* arc,
                                             SearchContext<ResourceType>& ctx) {
            using parts::ExtensionOutcome;
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

            auto& new_label = ctx.pool->get_next_label(head_node);
            Direction::extend(*label, *arc, &new_label);
            if (!Direction::feasible(new_label)) {
                ctx.pool->release_label(&new_label);  // never a predecessor: release at once
                observer.on_extension(*label, *arc, ExtensionOutcome::Infeasible);
                return ExtensionOutcome::Infeasible;
            }
            // The rule decides; the node view's members do the evicting.
            auto node_view = labels.node(head_node->pos());
            if (!admission.admit(new_label, node_view, ctx.admission)) {
                ctx.pool->release_label(&new_label);
                observer.on_extension(*label, *arc, ExtensionOutcome::Dominated);
                return ExtensionOutcome::Dominated;
            }
            schedule.push(
                {.label = &new_label, .position = labels.store(head_node->pos(), &new_label)});
            new_label.set_prev_label(label);  // pin the predecessor until this label is released
            observer.on_extension(*label, *arc, ExtensionOutcome::Accepted);
            return ExtensionOutcome::Accepted;
        }

        /// @brief Carries out what the take rule decided for a label it did not extend.
        ///
        /// @param taken   The label and its position in its node's container.
        /// @param verdict What to do with it.
        /// @param pool    The label pool.
        void apply(const parts::StoredLabel<ResourceType>& taken, Verdict verdict,
                   LabelPool<ResourceType>* pool) {
            switch (verdict) {
                case Verdict::Extend:
                case Verdict::Keep:  // an end-node, boundary or abandoned label stays stored
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
