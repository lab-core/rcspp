// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cmath>

namespace rcspp {

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

}  // namespace rcspp
