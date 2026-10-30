// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/solve_result.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp::detail {

/// @brief Why a bidirectional solve is running without the half-way point.
enum class HalfWayOff : unsigned {
    NoHalfWayPoint,      ///< the caller gave no `H` -- the default
    CriticalTypeAbsent,  ///< the critical resource type is not in the model
    NotAThreshold,       ///< the clock's backward values are not on the forward scale
    NotMonotone,         ///< the clock can decrease along an arc
    NotIncreasing,       ///< the clock is not an increasing part of the dominance order
    NoCeiling,           ///< the clock's extension sets no backward start at a sink
    IndexOutOfRange,     ///< `critical_resource_index` names no component of the critical type
};

/// @brief Whether this process has not yet reported the half-way point being off for @p reason.
///
/// Tracked per process, not per algorithm, so a pricing loop that builds a new algorithm for
/// every solve warns only once.
///
/// @param reason Why the point is off.
/// @return @c true the first time @p reason is reported.
inline bool first_report_of(HalfWayOff reason) {
    static std::atomic<unsigned> reported{0};
    const unsigned bit = 1U << static_cast<unsigned>(reason);
    return (reported.fetch_or(bit) & bit) == 0U;
}

/// @brief How many components of type @p ComponentType the model has, read off one arc's
///        extender.
///
/// Uses the arc @c threshold_probe::model_arc picks (a removed arc when no live one is left), so
/// the count does not depend on what preprocessing removed.
///
/// @return The count, or 0 when @p ComponentType is not in the model or no arc carries an
///         extender.
template <typename ComponentType, typename ResourceType>
[[nodiscard]] size_t number_of_components_of(const Graph<ResourceType>& graph) {
    if constexpr (is_cost_in_composition_v<ComponentType, ResourceType>) {
        const auto* arc = threshold_probe::model_arc(graph);
        return arc == nullptr ? 0 : arc->extender->template get_components<ComponentType>().size();
    } else {
        return 0;
    }
}

/// @brief The directional resource kind of the critical component, read off one arc's extender.
///
/// Only a `Threshold` backward value is on the forward scale that `H` uses; any other kind turns
/// the point off. Use e.g. `TimeWindowExtensionFunction` or `CapacityExtensionFunction`. One arc
/// suffices: all extenders come from one factory, so they share one component layout (assumed,
/// not checked). It is the arc the model checks read, live or removed.
///
/// @return The kind, or `Unspecified` when the graph has no arc carrying an extender.
template <typename CriticalResourceType, typename ResourceType>
[[nodiscard]] DirectionalResourceKind critical_directional_resource_kind(
    const Graph<ResourceType>& graph, size_t critical_resource_index) {
    const auto* arc = threshold_probe::model_arc(graph);
    if (arc == nullptr) {
        return DirectionalResourceKind::Unspecified;
    }
    return arc->extender->template get_component<CriticalResourceType>(critical_resource_index)
        .directional_resource_kind();
}

/// @brief A sink at which the clock's extension sets no backward start, if any.
///
/// A backward label there starts at the type default, below `H`, and would stop at once. The
/// start is read off the arc @c BackwardDirection::start_arc returns; a sink no live arc enters
/// starts no backward search.
///
/// @return The first such sink's id, or @c std::nullopt.
template <typename CriticalResourceType, typename ResourceType>
[[nodiscard]] std::optional<size_t> sink_without_a_ceiling(const Graph<ResourceType>& graph,
                                                           size_t critical_resource_index) {
    for (const size_t sink_id : graph.get_sink_node_ids()) {
        const auto* sink = graph.get_node(sink_id);
        if (sink == nullptr || sink->resource == nullptr) {
            continue;
        }
        const auto* entering = BackwardDirection::start_arc(graph, sink);
        if (entering == nullptr) {
            continue;
        }
        auto scratch =
            sink->resource->template get_component<CriticalResourceType>(critical_resource_index);
        if (!threshold_probe::observe_start(
                entering->extender->template get_component<CriticalResourceType>(
                    critical_resource_index),
                &scratch)) {
            return sink_id;
        }
    }
    return std::nullopt;
}

/// @brief Says, once per process and per reason, that the half-way point is off: at WARN the
///        first time, at DEBUG after that.
inline void report_half_way_off(HalfWayOff reason, const std::string& why) {
    if (first_report_of(reason)) {
        LOG_WARN("BidirectionalLabellingAlgorithm: the half-way point is off because ",
                 why,
                 ". The solve stays correct, but it runs a full forward search, a full backward "
                 "search and a join -- more work than a forward algorithm on the same model. "
                 "(Reported once per process; later solves log it at DEBUG.)\n");
    } else {
        LOG_DEBUG("BidirectionalLabellingAlgorithm: the half-way point is off because ",
                  why,
                  ".\n");
    }
}

/// @brief Why the half-way point must be off on this model, or none; a reason is also reported
///        (@ref report_half_way_off).
///
/// The seven checks, in this order: the critical type absent from the model; no
/// `half_way_point` given; the index out of range; not a threshold; no ceiling at a sink; not
/// monotone; not increasing in the dominance order. The model checks ran before the solve, so a
/// setting that does not fit the model turns the point off rather than throwing.
///
/// @tparam CriticalResourceType The critical resource's type.
/// @tparam ResourceType         The resource type carried by labels.
/// @return The reason, as a sentence, or @c std::nullopt when the point may be on.
template <typename CriticalResourceType, typename ResourceType>
[[nodiscard]] std::optional<std::string> half_way_off_reason(const Graph<ResourceType>& graph,
                                                             const AlgorithmBaseParams& params) {
    const auto off = [](HalfWayOff reason, std::string why) -> std::optional<std::string> {
        report_half_way_off(reason, why);
        return why;
    };
    if constexpr (!is_cost_in_composition_v<CriticalResourceType, ResourceType>) {
        return off(HalfWayOff::CriticalTypeAbsent,
                   "the critical resource type is not in the model");
    } else {
        const size_t index = params.critical_resource_index;
        const std::string clock = "critical resource " + std::to_string(index);
        if (params.half_way_point <= 0.0) {
            return off(HalfWayOff::NoHalfWayPoint,
                       "no half_way_point was given; set it to about half the critical "
                       "resource's range");
        }
        const size_t clocks = number_of_components_of<CriticalResourceType>(graph);
        if (index >= clocks) {
            return off(HalfWayOff::IndexOutOfRange,
                       clock + " does not exist: the model has " + std::to_string(clocks) +
                           " component(s) of the critical resource's type");
        }
        if (critical_directional_resource_kind<CriticalResourceType>(graph, index) !=
            DirectionalResourceKind::Threshold) {
            return off(HalfWayOff::NotAThreshold,
                       clock +
                           " does not extend backwards as a threshold, so its backward values "
                           "are not on the forward scale");
        }
        if (const auto sink = sink_without_a_ceiling<CriticalResourceType>(graph, index)) {
            return off(HalfWayOff::NoCeiling,
                       clock + " has no ceiling at sink " + std::to_string(*sink) +
                           ": its extension sets no backward start there, so a backward label "
                           "would start at the type default, below H");
        }
        const std::vector<double> probes{0.0, params.half_way_point};
        if (!critical_resource_is_monotone<CriticalResourceType>(graph, index, probes)) {
            return off(HalfWayOff::NotMonotone, clock + " is not monotone");
        }
        if (!critical_resource_dominance_is_increasing<CriticalResourceType>(graph, index)) {
            return off(HalfWayOff::NotIncreasing,
                       clock +
                           " does not take part in the dominance order as an increasing one, so "
                           "a dominator may sit above H while the label it evicted sat below it");
        }
        return std::nullopt;
    }
}

}  // namespace rcspp::detail
