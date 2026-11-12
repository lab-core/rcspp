// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/validation/threshold_probe.hpp"

namespace rcspp::detail {

/// @brief Where a bidirectional search's two sides stop: forward labels above `H`, backward
///        labels below it, on a critical resource that acts as a clock.
///
/// `H` is the half-way point: a value of the critical resource (the params'
/// @c critical_resource_index and @c half_way_point). The clock must be monotone (never decreasing
/// along an arc), so that each path crosses `H` on exactly one arc and the join produces it at
/// most once (a path whose clock never rises above `H` is found by the forward search instead).
/// The take rule reads @ref past for each side, and the join reads it for the forward labels it
/// pairs. Off, it holds no label back.
///
/// @tparam ResourceType The resource type carried by labels.
template <typename ResourceType>
class HalfWay {
    public:
        /// @brief Reads the clock's value from a label's resource.
        using reader_type = double (*)(const Resource<ResourceType>&, size_t);

        /// @brief Off.
        HalfWay() = default;

        /// @brief On, at @p point, reading the clock with @p read at @p index.
        HalfWay(double point, size_t index, reader_type read)
            : point_(point), on_(true), index_(index), read_(read) {}

        /// @brief Whether the half-way point is in force.
        [[nodiscard]] bool is_on() const { return on_; }

        /// @brief The half-way point `H`.
        [[nodiscard]] double point() const { return point_; }

        /// @brief Turns the half-way point off: both searches run to completion, correct but
        ///        slower.
        void turn_off() { on_ = false; }

        /// @brief Whether @p label, extended in @p Direction, is past the half-way point: above
        ///        `H` forward, below it backward. Off: never, and the clock is not read.
        template <typename Direction>
        [[nodiscard]] bool is_past(const Label<ResourceType>& label) const {
            if (!on_) {
                return false;
            }
            const double value = read_(label.get_resource(), index_);
            if constexpr (Direction::backward) {
                return value < point_;
            } else {
                return value > point_;
            }
        }

    private:
        double point_ = 0.0;
        bool on_ = false;
        size_t index_ = 0;
        reader_type read_ = nullptr;
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

/// @brief Probes whether the critical resource is monotone along every arc: for each probe
///        value, the extended value is not below it.
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
///         type is absent or the graph's first node carries no resource (only that node is
///         read).
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

}  // namespace rcspp::detail
