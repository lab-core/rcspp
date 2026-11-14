// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/parts/extension_outcome.hpp"
#include "rcspp/algorithm/labelling/parts/retention/retention.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/schedule.hpp"
#include "rcspp/algorithm/labelling/parts/stored_label.hpp"
#include "rcspp/algorithm/labelling/take_rule.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief Visits the schedule's next node: pulls into it, then runs the take rule on each label
///        just pulled, keeping it among the node's new labels or taking it off.
///
/// A label to extend stays among the new labels, where the node's successors pull it from. So
/// does an end-node label the rule keeps: nothing pulls from it, but it counts as waiting until
/// the next sweep drops it, as in the Pulling search this replaced. Any other is
/// dropped as the rule says and taken off. An end-node label the rule records goes to
/// @p record.
///
/// @return @c true when a recorded path ends the run (@ref SearchContext::should_end_run): the
///         caller ends its loop.
template <typename SearchType, typename RecordCallable, typename CountCallable>
bool step_node(SearchType& search, SearchContext<typename SearchType::resource_type>& context,
               const RecordCallable& record, const CountCallable& count) {
    using ResourceType = typename SearchType::resource_type;
    using Direction = typename SearchType::direction;
    using Rule = TakeRule<ResourceType, Direction>;
    static_assert(parts::PullingSchedule<typename SearchType::schedule_type, ResourceType>,
                  "step_node needs a pulling schedule (visit_next_node, current, remove_current)");

    search.schedule.visit_next_node(
        context,
        [&search, &context, &count](Label<ResourceType>* label, const Arc<ResourceType>* arc) {
            ++search.work;
            count(search.extend_along(label, arc, context));
        });
    auto& current = search.schedule.current();
    for (auto it = current.begin(); it != current.end();) {
        Decision decision = Rule::live(*it->label, search, context);
        if (decision.verdict == Verdict::Extend) {
            decision = Rule::fixed(*it->label, search, context);
        }
        if (decision.record) {
            record(*it->label);
            if (context.should_end_run()) {
                return true;
            }
        }
        // An end-node label the rule keeps stays among the new labels, as in the Pulling search
        // this replaced. Nothing pulls from an end node, so this changes no extension, but the
        // label counts as waiting (size()) until the next sweep drops it: a run
        // stopped early (max_iterations) then reads Truncated, as Pulling did. One the rule drops
        // (dominated, or pruned) is taken off like any other.
        const bool kept_at_end_node =
            decision.verdict == Verdict::Keep && Direction::is_end_node(it->label->get_end_node());
        if (decision.verdict == Verdict::Extend || kept_at_end_node) {
            ++it;
            continue;
        }
        search.apply(*it, decision.verdict, context.pool);
        it = search.schedule.remove_current(it);
    }
    return false;
}

/// @brief Runs one step of @p search, the unit a main loop counts against @c max_iterations: it
///        takes the next label from the schedule, runs the take rule on it, and either extends
///        it along every arc its direction walks or carries out the verdict (@ref Search::apply).
///        For a pulling schedule, a step visits the next node and pulls into it instead
///        (@ref step_node).
///
/// Once a label is extended, a schedule that places children (@c parts::PlacesChildren, a
/// dive's) places the labels just pushed; and a retention that keeps nothing behind
/// (@c parts::releases_taken_labels_v, a dive's) has the label released, as it has one the
/// rule keeps at an end node. Each branch is chosen at compile time: the other searches compile
/// to the step without them.
///
/// An end-node label the rule records goes to @p record; every extension's outcome goes to
/// @p count.
///
/// @param search The search, a @ref Search.
/// @param context    Its context.
/// @param record Callable `(const Label&)`: records an end-node label as a solution.
/// @param count  Callable `(parts::ExtensionOutcome)`: counts the extensions and logs the
///               progress.
/// @return @c true when a recorded path ends the run (@ref SearchContext::should_end_run): the
///         caller ends its loop.
template <typename SearchType, typename RecordCallable, typename CountCallable>
bool step(SearchType& search, SearchContext<typename SearchType::resource_type>& context,
          const RecordCallable& record, const CountCallable& count) {
    using ResourceType = typename SearchType::resource_type;
    using Direction = typename SearchType::direction;
    using Rule = TakeRule<ResourceType, Direction>;
    if constexpr (parts::PullingSchedule<typename SearchType::schedule_type, ResourceType>) {
        return step_node(search, context, record, count);
    } else {
        static_assert(parts::LabelSchedule<typename SearchType::schedule_type, ResourceType>,
                      "the schedule must be a label schedule (next) or a pulling schedule");

        const auto taken = search.schedule.next(context);
        if (taken.label == nullptr) {
            return false;
        }
        Decision decision = Rule::live(*taken.label, search, context);
        if (decision.verdict == Verdict::Extend) {
            decision = Rule::fixed(*taken.label, search, context);
        }
        bool run_ended = false;
        if (decision.record) {
            record(*taken.label);
            run_ended = context.should_end_run();
        }
        if (decision.verdict != Verdict::Extend) {
            search.apply(taken, decision.verdict, context.pool);
            return run_ended;
        }

        Label<ResourceType>* parent = taken.label;
        const auto arcs = Direction::out_arcs(*context.graph, parent->get_end_node());
        search.work += arcs.size();
        for (const auto* arc : arcs) {
            count(search.extend_along(parent, arc, context));
        }
        if constexpr (parts::PlacesChildren<typename SearchType::schedule_type, ResourceType>) {
            search.schedule.place_children(
                context,
                [&search](const LabelIterator<ResourceType>& position) { search.erase(position); });
        }
        if constexpr (parts::releases_taken_labels_v<typename SearchType::retention_type>) {
            // Its children pin it: it lives on while one of them waits.
            context.pool->release_with_ref_count(parent);
        }
        return false;
    }
}

}  // namespace rcspp::detail
