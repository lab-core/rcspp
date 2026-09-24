// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <atomic>
#include <cmath>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

/// @brief Why a bidirectional solve is running without the half-way bound.
enum class HalfWayOff : unsigned {
    NoHalfWayPoint,      ///< the caller gave no `H` -- the default
    CriticalTypeAbsent,  ///< the critical resource type is not in the model
    NotAThreshold,       ///< the clock's backward values are not on the forward scale
    NotMonotone,         ///< the clock can decrease along an arc
    NotIncreasing,       ///< the clock is not an increasing part of the dominance order
    NoCeiling,           ///< the clock states no ceiling at a sink to seed backward labels with
    IndexOutOfRange,     ///< `critical_resource_index` names no component of the critical type
};

/// @brief Whether this process has not yet reported the bound being off for @p reason.
///
/// Tracked per process, not per algorithm, so a pricing loop that builds a new algorithm for
/// every solve warns only once.
///
/// @param reason Why the bound is off.
/// @return @c true the first time @p reason is reported.
inline bool first_report_of(HalfWayOff reason) {
    static std::atomic<unsigned> reported{0};
    const unsigned bit = 1U << static_cast<unsigned>(reason);
    return (reported.fetch_or(bit) & bit) == 0U;
}

/// @brief Whether this process has not yet warned that `dynamic_half_way` is only reserved.
///
/// @return @c true the first time it is asked.
inline bool first_report_of_dynamic_half_way() {
    static std::atomic<bool> reported{false};
    return !reported.exchange(true);
}

/// @brief Decides where each direction's search stops.
///
/// Uses a critical resource that acts as a clock: monotone (never decreasing along an arc) and
/// bounded in `[0, R]`. Forward labels are discarded above the half-way point `H`, backward labels
/// below it. Monotonicity ensures each path crosses `H` on exactly one arc, so it is found exactly
/// once at the join; a non-monotone clock can silently lose paths. Cost is not a valid clock.
class HalfWayPolicy {
    public:
        /// @brief Constructs the policy, deriving `H` when it is not given explicitly.
        ///
        /// @param half_way_point       The value `H`. When 0, derived as `R / 2` if
        ///                             @p resource_upper_bound is finite (unlike
        ///                             @c AlgorithmParams::half_way_point, where 0 means "off").
        /// @param resource_upper_bound `R`, the critical resource's maximum. With no explicit `H`
        ///                             and no finite positive `R`, the bound starts disabled.
        ///                             A bidirectional solve passes an infinite `R` when its
        ///                             `half_way_point` is 0, so there `H` is never derived.
        HalfWayPolicy(double half_way_point, double resource_upper_bound) {
            if (half_way_point > 0.0) {
                h_ = half_way_point;
            } else if (std::isfinite(resource_upper_bound) && resource_upper_bound > 0.0) {
                h_ = resource_upper_bound / 2.0;
            } else {
                // No explicit H and no finite R.
                h_ = 0.0;
                enabled_ = false;
            }
        }

        /// @brief Whether a forward label has passed the half-way point and should be discarded.
        ///
        /// @param critical_value The label's value on the critical resource.
        /// @return `true` when the label is beyond `H` and the bound is in force.
        [[nodiscard]] bool should_stop_forward(double critical_value) const {
            return enabled_ && critical_value > h_;
        }

        /// @brief Whether a backward label is below the half-way point and should be discarded.
        ///
        /// Note the `<`: backward labels are discarded *below* `H`, the mirror of forward.
        ///
        /// @param critical_value The label's value on the critical resource.
        /// @return `true` when the label is short of `H` and the bound is in force.
        [[nodiscard]] bool should_stop_backward(double critical_value) const {
            return enabled_ && critical_value < h_;
        }

        /// @brief Turns the bound off: both searches run to completion, correct but slower.
        void disable() { enabled_ = false; }

        /// @brief Whether the bound is in force.
        [[nodiscard]] bool enabled() const { return enabled_; }

        /// @brief The half-way point `H`.
        [[nodiscard]] double h() const { return h_; }

    private:
        double h_ = 0.0;
        bool enabled_ = true;
};

/// @brief Checks that the chosen critical resource really is monotone along every arc.
///
/// For each arc and probe value `x`, extends a resource whose critical component is `x` and
/// requires the result to be at least `x`. Raw arc consumptions are not enough: an extension that
/// clamps up to a time window can be monotone despite a negative stored value.
///
/// @tparam CriticalRC    The critical resource's type.
/// @tparam ResourceTypes The graph's resource pack.
/// @param graph                   The graph whose arcs are probed.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @param probes                  Values to probe with, e.g. `{0, R/2}`.
/// @return `true` when every arc is monotone on the critical slot.
template <typename CriticalRC, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_is_monotone(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index,
    std::span<const double> probes) {
    // Type absent from the pack: report "not monotone" so the caller disables the bound.
    if constexpr (ComponentTypeIndex<CriticalRC, ResourceTypes...>::value == -1) {
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
                    source->template get_component<CriticalRC>(critical_resource_index);
                // Cast to the component's value type, which may be integral.
                using CriticalValueType =
                    std::decay_t<decltype(source_component.get_value().get_value())>;
                source_component.set_value(static_cast<CriticalValueType>(probe));

                arc.extender->extend(*source, extended.get());

                const auto& extended_component =
                    extended->template get_component<CriticalRC>(critical_resource_index);
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
/// The bound requires `X dominates Y => X.clock <= Y.clock`; otherwise a dominator past `H` can
/// evict a label the join needed. `ValueDominanceFunction` satisfies this;
/// `TrivialDominanceFunction` and higher-is-better orders do not.
///
/// @tparam CriticalRC    The critical resource's type.
/// @tparam ResourceTypes The graph's resource pack.
/// @param graph                   The graph whose nodes carry the function objects.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @return `true` when the clock's dominance is increasing; `false` otherwise, including when the
///         type is absent or the graph has no usable node.
template <typename CriticalRC, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_dominance_is_increasing(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index) {
    if constexpr (ComponentTypeIndex<CriticalRC, ResourceTypes...>::value == -1) {
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

        auto& low_component = low.template get_component<CriticalRC>(critical_resource_index);
        auto& high_component = high.template get_component<CriticalRC>(critical_resource_index);
        using CriticalValueType = std::decay_t<decltype(low_component.get_value().get_value())>;
        low_component.set_value(static_cast<CriticalValueType>(0));
        high_component.set_value(static_cast<CriticalValueType>(1));

        // Probe values one apart, well outside any comparison tolerance.
        return (low_component <= high_component) && !(high_component <= low_component);
    }
}

}  // namespace rcspp
