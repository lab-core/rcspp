// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/utils/logger.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp::detail {

/// @brief Where a bidirectional search's two sides stop: forward labels above `H`, backward
///        labels below it, on a critical resource that acts as a clock.
///
/// The clock must be monotone (never decreasing along an arc), so that each path crosses `H` on
/// exactly one arc and is found exactly once at the join; @ref half_way_off_reason checks it.
/// The take rule reads @ref past for each side, and the join reads it for the forward labels it
/// pairs. Off, it holds no label back.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class HalfWay {
    public:
        /// @brief Reads the clock's value from a label's resource.
        using Reader = double (*)(const Resource<ResourceType>&, size_t);

        /// @brief Off.
        HalfWay() = default;

        /// @brief On, at @p h, reading the clock with @p read at @p index.
        HalfWay(double h, size_t index, Reader read)
            : h_(h), on_(true), index_(index), read_(read) {}

        /// @brief Whether the half-way point is in force.
        [[nodiscard]] bool on() const { return on_; }

        /// @brief The half-way point `H`.
        [[nodiscard]] double h() const { return h_; }

        /// @brief Turns the half-way point off: both searches run to completion, correct but
        ///        slower.
        void turn_off() { on_ = false; }

        /// @brief Whether @p label, extended in @p Direction, is past the half-way point: above
        ///        `H` forward, below it backward. Off: never, and the clock is not read.
        template <typename Direction>
        [[nodiscard]] bool past(const Label<ResourceType>& label) const {
            if (!on_) {
                return false;
            }
            const double value = read_(label.get_resource(), index_);
            if constexpr (Direction::backward) {
                return value < h_;
            } else {
                return value > h_;
            }
        }

    private:
        double h_ = 0.0;
        bool on_ = false;
        size_t index_ = 0;
        Reader read_ = nullptr;
};

/// @brief The critical resource's scalar value in @p resource: the clock the half-way point reads.
///
/// @tparam CriticalResourceType The critical resource's type.
/// @tparam ResourceType         The resource type carried by labels.
/// @param resource                The label's resource.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @return The value, or 0 when @p CriticalResourceType is not in the composition (the point is
///         then off; the guard keeps that case compiling).
template <typename CriticalResourceType, typename ResourceType>
[[nodiscard]] double critical_value(const Resource<ResourceType>& resource,
                                    size_t critical_resource_index) {
    if constexpr (is_cost_in_composition_v<CriticalResourceType, ResourceType>) {
        const auto& component =
            resource.template get_component<CriticalResourceType>(critical_resource_index);
        return static_cast<double>(component.get_value().get_value());
    } else {
        return 0.0;
    }
}

/// @brief The half-way point the params ask for, reading @p CriticalResourceType: on when
///        `half_way_point > 0`, at `half_way_point`.
template <typename CriticalResourceType, typename ResourceType>
[[nodiscard]] HalfWay<ResourceType> make_half_way(const AlgorithmBaseParams& params) {
    if (params.half_way_point <= 0.0) {
        return {};
    }
    return {params.half_way_point,
            params.critical_resource_index,
            &critical_value<CriticalResourceType, ResourceType>};
}

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

/// @brief Checks that the chosen critical resource really is monotone along every arc.
///
/// For each arc and probe value `x`, extends a resource whose critical component is `x` and
/// requires the result to be at least `x`. Raw arc consumptions are not enough: an extension that
/// clamps up to a time window can be monotone despite a negative stored value.
///
/// @tparam CriticalResourceType The critical resource's type.
/// @tparam ResourceTypes        The graph's resource pack.
/// @param graph                   The graph whose arcs are probed.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @param probes                  Values to probe with, e.g. `{0, H}`.
/// @return `true` when every arc is monotone on the critical slot.
template <typename CriticalResourceType, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_is_monotone(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index,
    std::span<const double> probes) {
    // Type absent from the pack: report "not monotone" so the caller turns the point off.
    if constexpr (ComponentTypeIndex<CriticalResourceType, ResourceTypes...>::value == -1) {
        return false;
    } else {
        using Composed = Resource<ResourceTypeComposition<ResourceTypes...>>;
        // Two scratch resources, cloned once and rebound per arc: reset() adopts a node's
        // function objects without cloning them, as a pooled label does.
        std::unique_ptr<Composed> source;
        std::unique_ptr<Composed> extended;
        bool monotone = true;
        graph.for_each_arc([&](const auto& arc) {
            if (!monotone || arc.extender == nullptr || arc.origin->resource == nullptr ||
                arc.destination->resource == nullptr) {
                return;
            }
            if (source == nullptr) {
                source = std::make_unique<Composed>(*arc.origin->resource);
                extended = std::make_unique<Composed>(*arc.destination->resource);
            }
            for (const double probe : probes) {
                source->reset(*arc.origin->resource);
                extended->reset(*arc.destination->resource);

                auto& source_component =
                    source->template get_component<CriticalResourceType>(critical_resource_index);
                // Cast to the component's value type, which may be integral.
                using CriticalValueType =
                    std::decay_t<decltype(source_component.get_value().get_value())>;
                source_component.set_value(static_cast<CriticalValueType>(probe));

                arc.extender->extend(*source, extended.get());

                const auto& extended_component =
                    extended->template get_component<CriticalResourceType>(critical_resource_index);
                const auto after = static_cast<double>(extended_component.get_value().get_value());
                if (after < probe) {
                    monotone = false;
                    return;
                }
            }
        });
        return monotone;
    }
}

/// @brief Checks that the chosen critical resource takes part in the dominance order, increasing.
///
/// The half-way point requires `X dominates Y => X.clock <= Y.clock`; otherwise a dominator past
/// `H` can evict a label the join needed. `ValueDominanceFunction` satisfies this;
/// `TrivialDominanceFunction` and higher-is-better orders do not.
///
/// @tparam CriticalResourceType The critical resource's type.
/// @tparam ResourceTypes        The graph's resource pack.
/// @param graph                   The graph whose nodes carry the function objects.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @return `true` when the clock's dominance is increasing; `false` otherwise, including when the
///         type is absent or the graph has no usable node.
template <typename CriticalResourceType, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_dominance_is_increasing(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index) {
    if constexpr (ComponentTypeIndex<CriticalResourceType, ResourceTypes...>::value == -1) {
        return false;
    } else {
        const std::vector<size_t> node_ids = graph.get_node_ids();
        if (node_ids.empty()) {
            return false;
        }
        const auto* node = graph.get_node(node_ids.front());
        if (node == nullptr || node->resource == nullptr) {
            return false;
        }

        // Copies clone the function objects.
        Resource<ResourceTypeComposition<ResourceTypes...>> low(*node->resource);
        Resource<ResourceTypeComposition<ResourceTypes...>> high(*node->resource);

        auto& low_component =
            low.template get_component<CriticalResourceType>(critical_resource_index);
        auto& high_component =
            high.template get_component<CriticalResourceType>(critical_resource_index);
        using CriticalValueType = std::decay_t<decltype(low_component.get_value().get_value())>;
        low_component.set_value(static_cast<CriticalValueType>(0));
        high_component.set_value(static_cast<CriticalValueType>(1));

        // Probe values one apart, well outside any comparison tolerance.
        return (low_component <= high_component) && !(high_component <= low_component);
    }
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
[[nodiscard]] size_t components_of(const Graph<ResourceType>& graph) {
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
        LOG_WARN("BidirectionalLabellingAlgorithm: the half-way bound is off because ",
                 why,
                 ". The solve stays correct, but it runs a full forward search, a full backward "
                 "search and a join -- more work than a forward algorithm on the same model. "
                 "(Reported once per process; later solves log it at DEBUG.)\n");
    } else {
        LOG_DEBUG("BidirectionalLabellingAlgorithm: the half-way bound is off because ",
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
        const size_t clocks = components_of<CriticalResourceType>(graph);
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
