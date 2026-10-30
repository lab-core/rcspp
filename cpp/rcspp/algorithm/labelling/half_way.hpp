// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
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

}  // namespace rcspp::detail
