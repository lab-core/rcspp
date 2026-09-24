// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/base/resource.hpp"
#include "rcspp/resource/functions/backward_kind.hpp"
#include "rcspp/resource/functions/dominance/dominance_function.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/validation/model_check.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp {

/// @brief Checks that a backward search can read the model: every component, and the
///        composition, has a backward form, and the backward values mean what the forward search
///        enforces.
///
/// A backward or bidirectional search needs it. Its problems, in order:
///  - a component whose extension function declares no @c backward_kind();
///  - a @c Threshold component whose clamps or starts disagree with its feasibility function
///    (@ref check_threshold_clamps), and negative loads under a floor
///    (@ref scan_negative_loads);
///  - a composition dominance, extension or feasibility function with no backward form;
///  - a cost that is not additive: a backward label reaching a source is a complete path, so its
///    cost must be what the forward search would compute.
///
/// @tparam ResourceType The resource type used in the graph.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class BackwardExtensionCheck final : public ModelCheck<ResourceType> {
    public:
        [[nodiscard]] std::string_view name() const override { return "backward extension"; }

        /// @brief Pairs @c backward_kind() from one arc's extender with the other declarations of
        ///        one node's resource, by component index, then probes the model's functions.
        void check(const Graph<ResourceType>& graph, ModelReport* report) const override {
            const auto* arc = threshold_probe::model_arc(graph);
            const auto kinds = threshold_probe::model_kinds(graph);
            // No arcs at all: nothing to read the declarations from.
            if (kinds.empty()) {
                return;
            }

            // Any node with a resource answers for all; finding none is a refusal.
            const auto* node = threshold_probe::node_with_resource(graph);
            if (node == nullptr) {
                report->problems.emplace_back(
                    "no node carries a resource, so its backward declarations cannot be checked");
                return;
            }

            auto* problems = &report->problems;
            size_t index = 0;
            node->resource->for_each_component([&](const auto& component) {
                const auto kind = index < kinds.size() ? kinds[index] : BackwardKind::Unspecified;
                describe_problem(component, kind, index, problems);
                ++index;
            });
            const auto needs_scan = check_threshold_clamps(graph, kinds, problems);
            scan_negative_loads(graph, needs_scan, problems);

            // The composition dominance function must also have a backward form.
            try {
                static_cast<void>(node->resource->back_dominates(*node->resource));
            } catch (const NoBackwardDominance&) {
                problems->emplace_back(
                    "the model's composition dominance function has no backward form; override "
                    "check_back_dominance (CompositionDominanceFunction, the default, has one)");
            }

            // So must the composition extension and feasibility functions: probed once, before
            // any label exists, rather than failing in the backward search.
            try {
                Resource<ResourceType> probe(*node->resource);
                arc->extender->extend_back(*node->resource, &probe);
            } catch (const NoBackwardExtension&) {
                problems->emplace_back(
                    "the model's composition extension function has no backward form; override "
                    "extend_back (CompositionExtensionFunction, the default, has one)");
            }
            try {
                Resource<ResourceType> probe(*node->resource);
                arc->extender->start_back(&probe);
            } catch (const NoBackwardExtension&) {
                problems->emplace_back(
                    "the model's composition extension function has no backward start; override "
                    "start_back (CompositionExtensionFunction, the default, has one)");
            }
            try {
                static_cast<void>(node->resource->is_back_feasible());
            } catch (const NoBackwardFeasibility&) {
                problems->emplace_back(
                    "the model's composition feasibility function has no backward form; override "
                    "is_back_feasible (CompositionFeasibilityFunction, the default, has one)");
            }

            // The join, the backward terminal and the backward prune all add the two halves'
            // costs.
            if (!node->resource->is_cost_additive()) {
                problems->emplace_back(
                    "the model's cost function is not additive: the join adds the two halves' "
                    "costs, which is exact only when every component the cost reads has a "
                    "zero cost (TrivialCostFunction) or a value cost (ValueCostFunction) under an "
                    "accumulating extension; a threshold's backward value is a bound, not a cost");
            }
        }

    private:
        using ClampProbe = threshold_probe::ClampProbe<ResourceType>;
        using NodeClamps = threshold_probe::NodeClamps;

        /// @brief Records what, if anything, is wrong with one component's backward declarations.
        template <typename ComponentResource>
        static void describe_problem(const ComponentResource& component, BackwardKind kind,
                                     size_t index, std::vector<std::string>* problems) {
            const std::string label = "component " + std::to_string(index);
            if (kind == BackwardKind::Unspecified) {
                problems->push_back(label + ": its extension function declares no backward_kind()");
            }
            // A feasibility function that tests "is this node in my memory" needs a memory read
            // from the arc's endpoints; with an ArcValue one every backward label would be
            // rejected.
            if (component.requires_arc_endpoints() && kind != BackwardKind::ArcEndpoints) {
                problems->push_back(
                    label +
                    ": its feasibility function forbids each node at itself, which only reads "
                    "correctly backwards when the memory at a node excludes that node; its "
                    "extension function does not declare BackwardKind::ArcEndpoints, so going "
                    "backward the memory arrives already holding the node it sits on and every "
                    "backward extension is rejected. Derive the extension function from "
                    "ArcEndpointsForm -- NgPathExtensionFunction is the one this library ships, "
                    "and "
                    "presets::add_elementary_resource wires it up for an elementary path");
            }
        }

        /// @brief Records each @c Threshold component whose clamps disagree with its feasibility
        ///        function, and says which components the negative-load scan must check.
        ///
        /// Per node, it observes the clamps (@c threshold_probe::observe_node_clamps) and checks
        /// each component against them (@ref check_component_clamps):
        ///  - the backward clamp, and at a sink the backward start, against the node's backward
        ///    and forward tests (@ref check_ceiling);
        ///  - the lowest value a forward label can hold there against the node's backward test
        ///    (@ref check_floor).
        ///
        /// The clamps and starts are observed, not declared, so the checks see what the search
        /// applies, however the extension is written. The checks share one probe per node and
        /// direction.
        ///
        /// A component needs the negative-load scan when its feasibility function declares
        /// @c requires_nondecreasing(), or when it is a @c Threshold whose backward test has a
        /// floor and whose extension no forward clamp (see @ref check_floor).
        ///
        /// Costs one pass over the nodes, with two probes each and a third at a sink.
        ///
        /// @param graph    The graph.
        /// @param kinds    Each component's backward kind.
        /// @param problems Receives one line per offending component.
        /// @return For each component, whether @ref scan_negative_loads must check its arcs.
        [[nodiscard]] static std::vector<bool> check_threshold_clamps(
            const Graph<ResourceType>& graph, const std::vector<BackwardKind>& kinds,
            std::vector<std::string>* problems) {
            const size_t count = kinds.size();
            const auto removed = threshold_probe::index_removed_arcs(graph);
            std::unique_ptr<ClampProbe> probe;
            NodeClamps clamps(count);
            ClampCheckState state(count);
            for (const size_t node_id : graph.get_node_ids()) {
                const auto* node = graph.get_node(node_id);
                if (node == nullptr || node->resource == nullptr) {
                    continue;
                }
                if (probe == nullptr) {
                    probe = std::make_unique<ClampProbe>(*node->resource);
                }
                threshold_probe::observe_node_clamps(*node, removed, kinds, probe.get(), &clamps);

                size_t index = 0;
                node->resource->for_each_component([&](const auto& component) {
                    const size_t component_index = index++;
                    if (component_index < count) {
                        check_component_clamps(component,
                                               component_index,
                                               node_id,
                                               kinds[component_index],
                                               clamps,
                                               &state,
                                               problems);
                    }
                });
            }
            return state.needs_scan;
        }

        /// @brief Records a negative consumption on any arc, removed ones included, of a component
        ///        whose backward reading assumes the value never decreases.
        ///
        /// A backward label carries only a ceiling, so a path whose value dips below the floor
        /// inside the suffix would be accepted. Reported once per component.
        ///
        /// Costs one pass over the arcs when some component needs it, reading values only.
        ///
        /// @param graph      The graph.
        /// @param needs_scan For each component, whether to check it; see
        ///                   @ref check_threshold_clamps.
        /// @param problems   Receives one line per offending component.
        static void scan_negative_loads(const Graph<ResourceType>& graph,
                                        std::vector<bool> needs_scan,
                                        std::vector<std::string>* problems) {
            if (std::ranges::find(needs_scan, true) == needs_scan.end()) {
                return;
            }
            threshold_probe::for_each_model_arc(graph, [&](const Arc<ResourceType>& arc) {
                if (arc.extender == nullptr) {
                    return;
                }
                size_t index = 0;
                arc.extender->for_each_component([&](const auto& component) {
                    const size_t component_index = index++;
                    if (component_index >= needs_scan.size() || !needs_scan[component_index]) {
                        return;
                    }
                    using Value = std::decay_t<decltype(component.get_value())>;
                    if constexpr (is_numerical_resource_v<Value>) {
                        const auto consumption =
                            static_cast<double>(component.get_value().get_value());
                        if (consumption < 0.0) {
                            needs_scan[component_index] = false;  // report once
                            problems->push_back(
                                "component " + std::to_string(component_index) + ": arc " +
                                std::to_string(arc.origin->id) + " -> " +
                                std::to_string(arc.destination->id) + " consumes " +
                                std::to_string(consumption) +
                                ", but its backward reading assumes no arc lowers the value: a "
                                "backward label carries only a ceiling, so a path dipping below "
                                "the floor would be accepted; loads must be non-negative");
                        }
                    }
                });
            });
        }

        /// @brief What the clamp checks have found so far, per component.
        struct ClampCheckState {
                explicit ClampCheckState(size_t count)
                    : needs_scan(count, false),
                      ceiling_reported(count, false),
                      floor_reported(count, false) {}

                /// Whether @ref scan_negative_loads must check the component's arcs.
                std::vector<bool> needs_scan;
                /// Whether @ref check_ceiling reported the component already.
                std::vector<bool> ceiling_reported;
                /// Whether @ref check_floor reported the component already.
                std::vector<bool> floor_reported;
        };

        /// @brief Checks one component of one node against the clamps observed there.
        ///
        /// Marks the component for the negative-load scan when its feasibility function requires
        /// non-decreasing values; for a @c Threshold, runs @ref check_ceiling where a backward
        /// step can arrive and where a backward label starts, and @ref check_floor where a
        /// forward step can arrive.
        ///
        /// @param component       The node's resource component.
        /// @param component_index The component's index.
        /// @param node_id         The node.
        /// @param kind            The component's backward kind.
        /// @param clamps          The clamps observed at the node.
        /// @param state           What the checks have found so far.
        /// @param problems        Receives one line per offending component.
        template <typename ComponentResource>
        static void check_component_clamps(const ComponentResource& component,
                                           size_t component_index, size_t node_id,
                                           BackwardKind kind, const NodeClamps& clamps,
                                           ClampCheckState* state,
                                           std::vector<std::string>* problems) {
            if (component.requires_nondecreasing()) {
                state->needs_scan[component_index] = true;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            if constexpr (is_numerical_resource_v<Value>) {
                if (kind != BackwardKind::Threshold) {
                    return;
                }
                if (clamps.has_leaving) {
                    check_ceiling(component,
                                  clamps.backward[component_index],
                                  clamps.forward[component_index],
                                  component_index,
                                  node_id,
                                  /*start=*/false,
                                  &state->ceiling_reported,
                                  problems);
                }
                if (clamps.has_start) {
                    check_ceiling(component,
                                  clamps.start[component_index],
                                  clamps.forward[component_index],
                                  component_index,
                                  node_id,
                                  /*start=*/true,
                                  &state->ceiling_reported,
                                  problems);
                }
                if (clamps.has_entering && check_floor(component,
                                                       clamps.forward[component_index],
                                                       clamps.backward[component_index],
                                                       component_index,
                                                       node_id,
                                                       &state->floor_reported,
                                                       problems)) {
                    state->needs_scan[component_index] = true;
                }
            }
        }

        /// @brief Records a @c Threshold component whose backward clamp at @p node_id, or whose
        ///        backward start at sink @p node_id, disagrees with that node's feasibility
        ///        function, once per component.
        ///
        /// A backward value is a deadline: the latest value at which the rest of the path is still
        /// feasible. At a node it can be no later than the largest value the node's forward test
        /// admits, its ceiling, and it is that ceiling when nothing later lowers it. So the clamp
        /// of a backward step arriving at the node, and the start of a backward label at a sink,
        /// must both be the ceiling. Four disagreements:
        ///  - none, where the forward test has a ceiling (it rejects a value beyond every bound):
        ///    without a clamp, the backward search admits deadlines the forward search rejects;
        ///    without a start, the label keeps the type default and rejects deadlines a forward
        ///    path meets;
        ///  - one the node's backward test rejects, so every backward label there is lost;
        ///  - one the forward test rejects, above the ceiling: the backward search admits
        ///    deadlines the forward search rejects, which a test on the floor alone (a time
        ///    window's) cannot see;
        ///  - one with a value just above it (@c threshold_probe::just_above) the forward test
        ///    admits, below the ceiling: the backward search rejects deadlines a forward path
        ///    meets.
        ///
        /// The forward test is asked, not read, so it must be exact at its ceiling: one admitting
        /// values a little above it, beyond the library's own tolerance, is refused. Where the
        /// forward test has no ceiling, only the backward test is asked.
        ///
        /// A clamp below the lowest value a forward label can hold at the node
        /// (@c threshold_probe::lowest_forward_value) is left alone: the node's window is empty,
        /// so it cannot be reached in time whatever the ceiling, which is the model's business,
        /// not an incoherence.
        ///
        /// @param component        The node's resource component.
        /// @param ceiling          The clamp a backward step arriving at the node was observed to
        ///                         apply, or the start a backward label at the sink was observed
        ///                         to take; @c std::nullopt when there is none.
        /// @param forward_clamp    The clamp a forward step arriving at the node was observed to
        ///                         apply, if any.
        /// @param component_index  The component's index, for the message.
        /// @param node_id          The node, for the message.
        /// @param start            Whether @p ceiling is a sink's backward start, not a clamp.
        /// @param ceiling_reported Which components were reported already.
        /// @param problems         Receives the line, if any.
        template <typename ComponentResource>
        static void check_ceiling(const ComponentResource& component, std::optional<double> ceiling,
                                  const std::optional<double>& forward_clamp,
                                  size_t component_index, size_t node_id, bool start,
                                  std::vector<bool>* ceiling_reported,
                                  std::vector<std::string>* problems) {
            if ((*ceiling_reported)[component_index]) {
                return;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
            const auto admits = [&component](Scalar scalar) {
                Value value;
                value.set_value(scalar);
                return component.admits_value(value);
            };
            const bool has_ceiling = !admits(
                threshold_probe::beyond_bounds(Scalar{0}, /*backward=*/true, /*second=*/false));
            const std::string subject = "component " + std::to_string(component_index) +
                                        ": its backward labels at " + (start ? "sink " : "node ") +
                                        std::to_string(node_id);
            if (!ceiling) {
                if (has_ceiling) {
                    (*ceiling_reported)[component_index] = true;
                    problems->push_back(
                        subject +
                        (start ? " start at the type default" : " are not clamped at all") +
                        ", but its feasibility function rejects values above some bound there, so "
                        "the backward search " +
                        (start ? "rejects deadlines a forward path meets; start each backward "
                                 "label at its sink's upper bound, as ThresholdForm::start_back "
                                 "does"
                               : "admits deadlines the forward search rejects; clamp each "
                                 "backward label to its node's upper bound, as ThresholdForm "
                                 "does") +
                        ", built from the feasibility function's NodeBounds");
                }
                return;
            }
            if (*ceiling < threshold_probe::lowest_forward_value(forward_clamp)) {
                return;
            }
            const auto scalar = static_cast<Scalar>(*ceiling);
            Value value;
            value.set_value(scalar);
            std::string fault;
            if (!component.admits_back_value(value)) {
                fault =
                    ", which its feasibility function rejects there, so every backward label at "
                    "that node is lost";
            } else if (!has_ceiling) {
                return;
            } else if (!admits(scalar)) {
                fault =
                    ", above the largest value its feasibility function admits there, so the "
                    "backward search admits deadlines the forward search rejects";
            } else if (const Scalar above = threshold_probe::just_above(scalar);
                       above != scalar && admits(above)) {
                fault =
                    ", below the largest value its feasibility function admits there, so the "
                    "backward search rejects deadlines a forward path meets";
            } else {
                return;
            }
            (*ceiling_reported)[component_index] = true;
            problems->push_back(subject + (start ? " start at " : " are clamped to ") +
                                std::to_string(*ceiling) + fault +
                                "; build the extension and the feasibility function from one "
                                "NodeBounds (make_node_bounds, or the feasibility function's "
                                "bounds())");
        }

        /// @brief Records a @c Threshold component whose backward test at @p node_id rejects a
        ///        value a forward label can hold there, once per component.
        ///
        /// The backward search would then reject deadlines a forward path meets. The test is
        /// asked directly, at @c threshold_probe::lowest_forward_value: a floor at or below that
        /// value is met. A node whose backward clamp is below it has an empty window, which is
        /// left alone, as @ref check_ceiling leaves it.
        ///
        /// @param component       The node's resource component.
        /// @param forward_clamp   The forward clamp observed at the node, or @c std::nullopt when
        ///                        the extension applies none.
        /// @param backward_clamp  The backward clamp observed at the node, if any.
        /// @param component_index The component's index, for the message.
        /// @param node_id         The node, for the message.
        /// @param floor_reported  Which components were reported already.
        /// @param problems        Receives the line, if any.
        /// @return Whether the component relies on the negative-load scan: its backward test has
        ///         a floor, rejecting a value below every bound, and its extension no forward
        ///         clamp.
        template <typename ComponentResource>
        static bool check_floor(const ComponentResource& component,
                                const std::optional<double>& forward_clamp,
                                const std::optional<double>& backward_clamp, size_t component_index,
                                size_t node_id, std::vector<bool>* floor_reported,
                                std::vector<std::string>* problems) {
            if ((*floor_reported)[component_index]) {
                return false;
            }
            using Value = std::decay_t<decltype(component.get_value())>;
            using Scalar = std::decay_t<decltype(std::declval<Value>().get_value())>;
            const auto admits = [&component](Scalar scalar) {
                Value value;
                value.set_value(scalar);
                return component.admits_back_value(value);
            };
            if (admits(threshold_probe::beyond_bounds(Scalar{0},
                                                      /*backward=*/false,
                                                      /*second=*/false))) {
                return false;  // no floor at all
            }
            const double lowest = threshold_probe::lowest_forward_value(forward_clamp);
            const bool empty_window = backward_clamp && *backward_clamp < lowest;
            if (!empty_window && !admits(static_cast<Scalar>(lowest))) {
                (*floor_reported)[component_index] = true;
                problems->push_back(
                    "component " + std::to_string(component_index) +
                    ": its feasibility function rejects a backward value of " +
                    std::to_string(lowest) + " at node " + std::to_string(node_id) +
                    ", the lowest value a forward label can hold there, so the backward search "
                    "rejects deadlines a forward path meets; build the extension and the "
                    "feasibility function from the same windows (TimeWindowExtensionFunction with "
                    "TimeWindowFeasibilityFunction), or pair CapacityExtensionFunction with "
                    "MinMaxFeasibilityFunction(0, capacity)");
                return false;
            }
            return !forward_clamp;
        }
};

}  // namespace rcspp
