// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>
#include <span>
#include <type_traits>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

/// @brief Decides where each direction's search stops.
///
/// Bidirectional labeling only pays off if both searches stop in the middle, and to stop in the
/// middle you need something to measure "middle" with: one resource behaving like a **clock** --
/// monotone (never decreasing along an arc) and bounded (living in a known finite range `[0, R]`).
/// Time is the usual choice; a bounded load works; **cost does not**, because reduced costs go
/// negative.
///
/// Given a half-way point `H`, the forward search discards labels whose clock exceeds `H` and the
/// backward search discards labels whose clock falls below it. Monotonicity is what makes that
/// safe: along any path the clock only rises, so it crosses `H` on exactly one arc, and each
/// complete path therefore appears exactly once at the join. If the clock can *decrease* it may
/// cross `H` several times, and a valid path can be discarded by both searches and never found --
/// while the solver reports success.
///
/// Static `H` in v1. This is an object rather than a pair of numbers so a dynamic variant can
/// replace it later without re-plumbing the algorithm.
class HalfWayPolicy {
    public:
        /// @brief Constructs the policy, deriving `H` when it is not given explicitly.
        ///
        /// @param half_way_point       The value `H`. When 0, derived as `R / 2`.
        /// @param resource_upper_bound `R`, the critical resource's finite maximum. When this is
        ///                             not finite and positive and no explicit `H` is supplied,
        ///                             there is no middle to aim at and the bound starts disabled.
        HalfWayPolicy(double half_way_point, double resource_upper_bound) {
            if (half_way_point > 0.0) {
                h_ = half_way_point;
            } else if (std::isfinite(resource_upper_bound) && resource_upper_bound > 0.0) {
                h_ = resource_upper_bound / 2.0;
            } else {
                // No explicit H and no finite R: nothing to derive a middle from.
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
        /// Note the `<`: forward labels are discarded *above* `H`, backward labels *below* it.
        /// Reversing this discards exactly the labels the join needs and keeps the ones it does
        /// not, which shows up as an empty join rather than as an obvious error.
        ///
        /// @param critical_value The label's value on the critical resource.
        /// @return `true` when the label is short of `H` and the bound is in force.
        [[nodiscard]] bool should_stop_backward(double critical_value) const {
            return enabled_ && critical_value < h_;
        }

        /// @brief Turns the bound off, leaving the search correct but slow.
        ///
        /// Both searches then run to completion and the join considers every pair. That is the
        /// right failure mode: a *wrong* bound loses optimal solutions while reporting success,
        /// whereas a missing bound only costs time.
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
/// Probes the property directly rather than reading raw arc consumptions. A stored arc value can be
/// negative on a monotone extension -- one that clamps the result up to a node's opening time, say
/// -- and positive on a non-monotone one, so the stored value does not answer the question. There
/// is also no clean per-component accessor for an arc's extender.
///
/// For each arc and each probe value `x`, this sets the critical component of a source resource to
/// `x`, extends it along the arc, and requires the critical component of the result to be at least
/// `x`. It uses only accessors that already exist -- `arc.extender->extend(...)` and
/// `Resource::get_component<...>(index)` -- both of which `BellmanFordAlgorithm` uses the same way.
///
/// @tparam CriticalRC   The critical resource's *type*; the index alone does not identify a slot.
/// @tparam ResourceTypes The graph's resource pack.
/// @param graph                  The graph whose arcs are probed.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @param probes                 Values to probe with; `{0, R/2}` is enough to catch a sign error.
/// @return `true` when every arc is monotone on the critical slot.
template <typename CriticalRC, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_is_monotone(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index,
    std::span<const double> probes) {
    // The same guard AStarDominanceAlgorithm uses when its cost type is absent from the pack: with
    // no such component there is nothing to measure, so report "not monotone" and let the caller
    // disable the bound rather than fail to compile or throw.
    if constexpr (ComponentTypeIndex<CriticalRC, ResourceTypes...>::value == -1) {
        return false;
    } else {
        bool monotone = true;
        graph.for_each_arc([&](const auto& arc) {
            if (!monotone || arc.extender == nullptr || arc.origin->resource == nullptr ||
                arc.destination->resource == nullptr) {
                return;
            }
            for (const double probe : probes) {
                // Copy both endpoints' resources: the copy constructor clones the function
                // objects, which is what BellmanFordAlgorithm does for the same reason.
                Resource<ResourceTypeComposition<ResourceTypes...>> source(*arc.origin->resource);
                Resource<ResourceTypeComposition<ResourceTypes...>> extended(
                    *arc.destination->resource);

                auto& source_component =
                    source.template get_component<CriticalRC>(critical_resource_index);
                // Cast through the component's own value type: the pack may hold integral
                // resources, and set_value forwards straight to ResourceType::set_value.
                using CriticalValueType =
                    std::decay_t<decltype(source_component.get_value().get_value())>;
                source_component.set_value(static_cast<CriticalValueType>(probe));

                arc.extender->extend(source, &extended);

                const auto& extended_component =
                    extended.template get_component<CriticalRC>(critical_resource_index);
                const double after =
                    static_cast<double>(extended_component.get_value().get_value());
                if (after < probe) {
                    monotone = false;
                    return;
                }
            }
        });
        return monotone;
    }
}

}  // namespace rcspp
