// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/functions/node_bounds.hpp"

namespace rcspp {

/// @brief Feasibility function that enforces a [min, max] bound on a scalar resource value,
///        with optional per-node override bounds.
///
/// At each graph node the active window [min_, max_] is either the global default or the
/// per-node override supplied at construction.  The resource is feasible when
/// `min_ <= resource.value <= max_`.
///
/// The bidirectional join test depends on the paired extension function's backward kind; see
/// @ref join_rule. Under a threshold extension, build both functions from one
/// @ref SharedNodeBounds (see @ref bounds), so the extension clamps backward labels to the same
/// per-node maxima this function enforces forward.
///
/// @tparam ResourceType The resource type whose value supports `geq()`, `leq()`, and
///         `get_value()`.
/// @tparam ValueType The scalar type of the resource value; deduced from
///         `ResourceType::get_value()`.
template <typename ResourceType,
          typename ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>>
class MinMaxFeasibilityFunction
    : public Clonable<MinMaxFeasibilityFunction<ResourceType, ValueType>,
                      FeasibilityFunction<ResourceType>> {
    public:
        /// @brief Constructs the function with default global bounds and optional per-node
        ///        overrides.
        ///
        /// When `min_max_by_node_id` is empty, every node uses `[default_min, default_max]`.
        ///
        /// @param default_min Default lower bound applied at nodes without a specific override.
        /// @param default_max Default upper bound applied at nodes without a specific override.
        /// @param min_max_by_node_id Map from node id to a `{min, max}` pair that overrides the
        ///        default for that node.
        MinMaxFeasibilityFunction(
            ValueType default_min, ValueType default_max,
            std::map<size_t, std::pair<ValueType, ValueType>> min_max_by_node_id = {})
            : MinMaxFeasibilityFunction(
                  make_node_bounds(default_min, default_max, std::move(min_max_by_node_id))) {}

        /// @brief Constructs the function on shared per-node `{min, max}` windows.
        ///
        /// Pass the same object to the paired extension function (e.g.
        /// @c CapacityExtensionFunction), or read it back through @ref bounds.
        ///
        /// @param bounds The windows; must not be null.
        /// @throws std::invalid_argument If @p bounds is null.
        explicit MinMaxFeasibilityFunction(SharedNodeBounds<ValueType> bounds)
            : bounds_(std::move(bounds)) {
            if (bounds_ == nullptr) {
                throw std::invalid_argument("MinMaxFeasibilityFunction: bounds must not be null");
            }
            min_ = bounds_->default_lower();
            max_ = bounds_->default_upper();
            cache_join_bounds();
        }

        /// @brief The per-node windows this function enforces, to share with the paired
        ///        extension function.
        ///
        /// @return The shared windows.
        [[nodiscard]] auto bounds() const -> const SharedNodeBounds<ValueType>& { return bounds_; }

        /// @brief Checks that the resource value lies within [min_, max_].
        ///
        /// @param resource The resource to evaluate.
        /// @return `true` if the resource value satisfies both the lower and upper bound.
        [[nodiscard]] auto is_feasible(const ResourceType& resource) -> bool override {
            return resource.geq(min_) && resource.leq(max_);
        }

        /// @brief How the join tests this resource, which depends on the paired extension:
        ///
        ///  - @c Threshold: the backward value is a ceiling, so @c ValueOrder
        ///    (`forward <= ceiling`); @c Unspecified if any minimum is non-zero, since a floor is
        ///    never checked on the backward side.
        ///  - @c Accumulate: the backward value is the suffix's consumption, so @c Custom
        ///    (`forward + backward <= capacity`, see @ref can_be_joined), but only for a uniform
        ///    window with a zero minimum. Otherwise the sum cannot be exact, so @c Unspecified.
        ///  - Otherwise (including unpaired): @c Unspecified, so bidirectional refuses.
        ///
        /// @return The join rule implied by the paired extension function.
        [[nodiscard]] JoinRule join_rule() const override {
            switch (this->backward_kind_) {
                case BackwardKind::Threshold:
                    return has_floor_ ? JoinRule::Unspecified : JoinRule::ValueOrder;
                case BackwardKind::Accumulate:
                    return accumulate_join_is_exact_ ? JoinRule::Custom : JoinRule::Unspecified;
                default:
                    return JoinRule::Unspecified;
            }
        }

        /// @brief The join test under an accumulation: the two halves' consumptions together must
        ///        fit under the cap.
        ///
        /// Only declared for a uniform `[0, max]` window (see @ref join_rule), where the sum is the
        /// joined path's largest value and the test is exact. Assumes a cumulative (never
        /// decreasing, unclamped) extension. A bidirectional setup checks that the extension adds.
        ///
        /// @param resource      The forward label's resource at the join node.
        /// @param back_resource The backward label's resource at the join node.
        /// @return `true` if the two consumptions together fit under the cap.
        [[nodiscard]] auto can_be_joined(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            if (this->backward_kind_ != BackwardKind::Accumulate) {
                throw std::logic_error(
                    "MinMaxFeasibilityFunction::can_be_joined is the accumulating form's body; a "
                    "threshold pairing joins through JoinRule::ValueOrder and an unpaired "
                    "function declares JoinRule::Unspecified");
            }
            return static_cast<ValueType>(resource.get_value() + back_resource.get_value()) <=
                   bounds_->default_upper();
        }

        /// @brief The accumulating join test adds the two halves, which bounds the path's
        ///        largest value only if the value never decreases.
        ///
        /// @return @c true under an accumulating extension.
        [[nodiscard]] auto requires_nondecreasing() const -> bool override {
            return this->backward_kind_ == BackwardKind::Accumulate;
        }

    private:
        SharedNodeBounds<ValueType> bounds_;
        ValueType min_{};
        ValueType max_{};

        /// @brief Whether the accumulating join test is exact: one window for every node, with a
        ///        zero minimum.
        bool accumulate_join_is_exact_ = true;

        /// @brief Whether any minimum is non-zero. A backward label carries only an upper limit, so
        ///        a threshold's join cannot check a minimum.
        bool has_floor_ = false;

        /// @brief Computes the two flags above, once, from the whole model: every node's resource
        ///        must agree on the join rule.
        void cache_join_bounds() {
            has_floor_ = bounds_->default_lower() != ValueType{};
            // A non-zero minimum rejects the empty suffix a backward label starts from, and a
            // per-node window bounds what the path has consumed up to a node, which a backward
            // label cannot know.
            accumulate_join_is_exact_ = bounds_->is_uniform() && !has_floor_;
            for (const auto& [node_id, window] : bounds_->by_node()) {
                has_floor_ = has_floor_ || window.first != ValueType{};
            }
        }

        void preprocess(size_t node_id) override {
            if (bounds_->is_uniform()) {
                return;
            }
            std::tie(min_, max_) = bounds_->at(node_id);
        }
};

}  // namespace rcspp
