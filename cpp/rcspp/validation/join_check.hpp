// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"
#include "rcspp/resource/functions/cost/cost_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/validation/model_check.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp {

/// @brief Checks that the join of a bidirectional search can splice a forward and a backward
///        half into one path: every component declares how to join them, and the halves it adds
///        are sums.
///
/// A bidirectional search needs it, after @c BackwardExtensionCheck. Its problems, in order:
///  - a component whose feasibility function declares no @c join_rule(), or a join rule that does
///    not fit the component (@c JoinRule::ValueOrder on a non-scalar, or under an accumulation);
///  - an accumulating component whose two halves the join adds although its extension is not a
///    sum, or whose backward labels do not start at the type default (@ref check_accumulations);
///  - a composition feasibility function with no join test, probed only when this check found
///    nothing else, since an undeclared join rule would make the probe throw for that.
///
/// It reports nothing for a model with no node resource: @c BackwardExtensionCheck refuses that.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class JoinCheck final : public ModelCheck<ResourceType> {
    public:
        [[nodiscard]] std::string_view name() const override { return "join"; }

        /// @brief Pairs @c backward_kind() from one arc's extender with @c join_rule() and the
        ///        other declarations of one node's resource, by component index, then checks the
        ///        accumulations and probes the composition's join test.
        void check(const Graph<ResourceType>& graph, ModelReport* report) const override {
            const auto kinds = threshold_probe::model_kinds(graph);
            if (kinds.empty()) {
                return;
            }
            const auto* node = threshold_probe::node_with_resource(graph);
            if (node == nullptr) {
                return;
            }

            auto* problems = &report->problems;
            const size_t reported_before = problems->size();
            size_t index = 0;
            node->resource->for_each_component([&](const auto& component) {
                const auto kind = index < kinds.size() ? kinds[index] : BackwardKind::Unspecified;
                describe_problem(component, kind, index, problems);
                ++index;
            });
            check_accumulations(graph, *node->resource, kinds, problems);

            // The composition feasibility function needs a join test too. Probed only when this
            // check found nothing else: a component without a join rule would make it throw.
            if (problems->size() == reported_before) {
                try {
                    static_cast<void>(node->resource->can_be_joined(*node->resource));
                } catch (const NoBackwardFeasibility&) {
                    problems->emplace_back(
                        "the model's composition feasibility function has no join test; override "
                        "can_be_joined (CompositionFeasibilityFunction, the default, has one)");
                }
            }
        }

    private:
        using RemovedArcs = threshold_probe::RemovedArcs<ResourceType>;

        /// @brief Records what, if anything, is wrong with one component's join declarations.
        template <typename ComponentResource>
        static void describe_problem(const ComponentResource& component, BackwardKind kind,
                                     size_t index, std::vector<std::string>* problems) {
            using ComponentValue = std::decay_t<decltype(component.get_value())>;
            const auto rule = component.join_rule();
            const std::string label = "component " + std::to_string(index);

            if (rule == JoinRule::Unspecified) {
                problems->push_back(label + ": its feasibility function declares no join_rule()");
            }
            if constexpr (!is_numerical_resource_v<ComponentValue>) {
                if (rule == JoinRule::ValueOrder) {
                    problems->push_back(label +
                                        ": its feasibility function declares "
                                        "JoinRule::ValueOrder, which compares two scalar "
                                        "values, on a resource that has none; declare "
                                        "JoinRule::Custom with a body of its own");
                }
            }
            // ValueOrder reads the backward value as a bound; under an accumulation it is a
            // consumption, so the join would accept infeasible splices.
            if (kind == BackwardKind::Accumulate && rule == JoinRule::ValueOrder) {
                problems->push_back(
                    label +
                    ": its extension accumulates but its feasibility function declares "
                    "JoinRule::ValueOrder, which compares a prefix value against a suffix "
                    "value rather than against a bound; use a Threshold extension (e.g. "
                    "CapacityExtensionFunction) or declare JoinRule::Custom with a body that adds "
                    "the two halves");
            }
        }

        /// @brief Records each accumulating component whose two halves the join adds although its
        ///        extension is not a sum, once per component.
        ///
        /// A component's halves are added where its cost is its value (see @c is_cost_additive), or
        /// where its feasibility function reads the backward value as a running total
        /// (@c requires_nondecreasing, as @c MinMaxFeasibilityFunction does under an accumulation:
        /// its join tests `forward + backward`). That is exact only if a path's value is the sum
        /// of one amount per arc, which @c BackwardKind::Accumulate does not promise: a
        /// bottleneck, the largest load seen, extends backward the same way, but does not add.
        ///
        /// Checked by running the extension, as the clamps are (@ref check_sums), and by checking
        /// that such a component's backward labels start where the forward ones do
        /// (@ref check_accumulation_starts).
        ///
        /// @param graph    The graph.
        /// @param any      Any node's resource, to read the declarations from and copy probes of.
        /// @param kinds    Each component's backward kind.
        /// @param problems Receives one line per offending component.
        static void check_accumulations(const Graph<ResourceType>& graph,
                                        const Resource<ResourceType>& any,
                                        const std::vector<BackwardKind>& kinds,
                                        std::vector<std::string>* problems) {
            std::vector<bool> halves_added(kinds.size(), false);
            size_t index = 0;
            any.for_each_component([&](const auto& component) {
                const size_t component_index = index++;
                using Value = std::decay_t<decltype(component.get_value())>;
                if constexpr (is_numerical_resource_v<Value>) {
                    if (component_index < kinds.size() &&
                        kinds[component_index] == BackwardKind::Accumulate) {
                        halves_added[component_index] = component.cost_form() == CostForm::Value ||
                                                        component.requires_nondecreasing();
                    }
                }
            });
            if (std::ranges::find(halves_added, true) == halves_added.end()) {
                return;
            }
            const auto removed = threshold_probe::index_removed_arcs(graph);
            check_sums(graph, any, removed, &halves_added, problems);
            check_accumulation_starts(graph, any, removed, halves_added, problems);
        }

        /// @brief The sum check of @ref check_accumulations: from the type default, two steps
        ///        must give the sum of the two single steps.
        ///
        /// For each arc `a` of the model, and an arc `b` leaving its destination (or `a` again),
        /// `extend(extend(0, a), b)` must be `extend(0, a) + extend(0, b)`, exactly for an
        /// integral value and to a relative 1e-9 for floating point. Each step starts from a
        /// cleared output, as a label's does, since an extension may write nothing. The model's
        /// own arcs are a sample, not a proof: a step that is a sum on these values only passes.
        ///
        /// Costs one pass over the arcs, with three steps each.
        ///
        /// @param graph    The graph.
        /// @param any      Any node's resource.
        /// @param removed  The removed arcs, for a node whose live arcs are all gone.
        /// @param checked  Which components to check; one is cleared once reported.
        /// @param problems Receives one line per offending component.
        static void check_sums(const Graph<ResourceType>& graph, const Resource<ResourceType>& any,
                               const RemovedArcs& removed, std::vector<bool>* checked,
                               std::vector<std::string>* problems) {
            Resource<ResourceType> start(any);
            start.reset(any);
            Resource<ResourceType> first(any);
            Resource<ResourceType> second(any);
            Resource<ResourceType> direct(any);
            const auto step = [&](Resource<ResourceType>* output,
                                  const Resource<ResourceType>& input,
                                  const Arc<ResourceType>& arc) {
                output->reset(any);
                size_t index = 0;
                output->for_each_component(input,
                                           *arc.extender,
                                           [&](auto& out, const auto& in, const auto& extender) {
                                               if ((*checked)[index++]) {
                                                   extender.extend(in, &out);
                                               }
                                           });
            };
            threshold_probe::for_each_model_arc(graph, [&](const Arc<ResourceType>& arc) {
                if (arc.extender == nullptr ||
                    std::ranges::find(*checked, true) == checked->end()) {
                    return;
                }
                const auto* next = threshold_probe::arc_to_probe(arc.destination->out_arcs,
                                                                 removed.leaving,
                                                                 arc.destination->id);
                const Arc<ResourceType>& then = next != nullptr ? *next : arc;
                step(&first, start, arc);
                step(&second, first, then);
                step(&direct, start, then);
                size_t index = 0;
                second.for_each_component(
                    first,
                    direct,
                    [&](const auto& both, const auto& one, const auto& other) {
                        const size_t component_index = index++;
                        using Value = std::decay_t<decltype(both.get_value())>;
                        if constexpr (is_numerical_resource_v<Value>) {
                            if (!(*checked)[component_index]) {
                                return;
                            }
                            const auto two_steps = both.get_value().get_value();
                            const auto first_step = one.get_value().get_value();
                            const auto second_step = other.get_value().get_value();
                            if (is_sum(two_steps, first_step, second_step)) {
                                return;
                            }
                            (*checked)[component_index] = false;  // report once
                            problems->push_back(
                                "component " + std::to_string(component_index) +
                                ": its extension declares BackwardKind::Accumulate, but along " +
                                std::to_string(arc.origin->id) + " -> " +
                                std::to_string(arc.destination->id) + " -> " +
                                std::to_string(then.destination->id) +
                                " it is not a sum: the two steps give " +
                                std::to_string(two_steps) + ", the single steps " +
                                std::to_string(first_step) + " and " + std::to_string(second_step) +
                                "; its cost, or its feasibility function's join test, adds the two "
                                "halves, which is exact only for a sum: write the step as an "
                                "addition, or give the component a zero cost (TrivialCostFunction) "
                                "and a join test that does not add");
                        }
                    });
            });
        }

        /// @brief Whether @p both is @p one plus @p other: exactly for an integral type, to a
        ///        relative 1e-9 for floating point, where an infinite sum (an arc that costs
        ///        infinity, say) must match exactly.
        ///
        /// @param both  The value two steps give.
        /// @param one   The value the first step gives alone.
        /// @param other The value the second step gives alone.
        /// @return Whether the two steps add up.
        template <typename Scalar>
        [[nodiscard]] static bool is_sum(Scalar both, Scalar one, Scalar other) {
            const Scalar sum = one + other;
            if (both == sum) {
                return true;
            }
            if constexpr (std::is_floating_point_v<Scalar>) {
                constexpr Scalar kRelative = 1e-9;
                return std::abs(both - sum) <=
                       kRelative * std::max({Scalar{1}, std::abs(both), std::abs(sum)});
            } else {
                return false;
            }
        }

        /// @brief The start check of @ref check_accumulations: at each sink, a checked component
        ///        must leave the backward start at the type default, where the forward search
        ///        starts, or the join counts the start on top of the path's own sum.
        ///
        /// Read through an arc entering the sink, as a backward search seeds a sink
        /// (@c threshold_probe::observe_start).
        ///
        /// @param graph    The graph.
        /// @param any      Any node's resource.
        /// @param removed  The removed arcs, for a node whose live arcs are all gone.
        /// @param checked  Which components to check.
        /// @param problems Receives one line per offending component.
        static void check_accumulation_starts(const Graph<ResourceType>& graph,
                                              const Resource<ResourceType>& any,
                                              const RemovedArcs& removed,
                                              const std::vector<bool>& checked,
                                              std::vector<std::string>* problems) {
            std::vector<bool> reported(checked.size(), false);
            Resource<ResourceType> scratch(any);
            for (const size_t sink_id : graph.get_sink_node_ids()) {
                const auto* sink = graph.get_node(sink_id);
                if (sink == nullptr) {
                    continue;
                }
                const auto* entering =
                    threshold_probe::arc_to_probe(sink->in_arcs, removed.entering, sink_id);
                if (entering == nullptr) {
                    continue;
                }
                size_t index = 0;
                scratch.for_each_component(
                    *entering->extender,
                    [&](auto& component, const auto& extender) {
                        const size_t component_index = index++;
                        if (component_index >= checked.size() || !checked[component_index] ||
                            reported[component_index]) {
                            return;
                        }
                        if (const auto start =
                                threshold_probe::observe_start(*extender, &component)) {
                            reported[component_index] = true;
                            problems->push_back(
                                "component " + std::to_string(component_index) +
                                ": its extension declares BackwardKind::Accumulate, but starts "
                                "its backward labels at sink " +
                                std::to_string(sink_id) + " at " + std::to_string(*start) +
                                ", not at the type default the forward search starts from, so "
                                "the join counts the start on top of the path's own sum; leave "
                                "start_back to its default");
                        }
                    });
            }
        }
};

}  // namespace rcspp
