// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/algorithm/labelling/context.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/take_rule.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/label/label.hpp"

namespace rcspp::detail {

/// @brief Carries out one unit of @p search's work: takes the next label from its schedule and
///        carries out what the take rule decides (extends it along every arc its direction
///        walks, or drops it).
///
/// An end-node label the rule records goes to @p record; every extension's outcome goes to
/// @p count.
///
/// @param search The search, a @ref Search.
/// @param ctx    Its context.
/// @param record Callable `(const Label&)`: records an end-node label as a solution.
/// @param count  Callable `(parts::ExtensionOutcome)`: counts the extensions and logs the
///               progress.
/// @return @c true when a budget is spent (@c max_solutions): the caller ends its loop.
template <typename SearchType, typename RecordCallable, typename CountCallable>
bool step(SearchType& search, SearchContext<typename SearchType::resource_type>& ctx,
          const RecordCallable& record, const CountCallable& count) {
    using ResourceType = typename SearchType::resource_type;
    using Direction = typename SearchType::direction;
    using Rule = TakeRule<ResourceType, Direction>;
    static_assert(parts::LabelSchedule<typename SearchType::schedule_type, ResourceType>,
                  "the schedule must hand out one label at a time");

    const auto taken = search.schedule.next(ctx);
    if (taken.label == nullptr) {
        return false;
    }
    Decision decision = Rule::live(*taken.label, search, ctx);
    if (decision.verdict == Verdict::Extend) {
        decision = Rule::settled(*taken.label, search, ctx);
    }
    bool spent = false;
    if (decision.record) {
        record(*taken.label);
        spent = ctx.budget_spent();
    }
    if (decision.verdict != Verdict::Extend) {
        search.apply(taken, decision.verdict, ctx.pool);
        return spent;
    }

    Label<ResourceType>* parent = taken.label;
    const auto arcs = Direction::out_arcs(*ctx.graph, parent->get_end_node());
    search.work += arcs.size();
    for (const auto* arc : arcs) {
        count(search.extend_along(parent, arc, ctx));
    }
    return false;
}

}  // namespace rcspp::detail
