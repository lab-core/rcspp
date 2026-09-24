// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <optional>
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
/// @ref merge_rule. Under a threshold extension, build both functions from one
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
        /// @brief Constructs the function with a single global [min, max] window.
        ///
        /// No per-node overrides; every node uses the same bounds.
        ///
        /// @deprecated The flag is ignored and kept only so existing calls compile. The merge test
        ///             and the backward seed follow the paired extension; see @ref merge_rule and
        ///             @ref back_seed_value.
        ///
        /// @param min Global lower bound on the resource value.
        /// @param max Global upper bound on the resource value.
        MinMaxFeasibilityFunction(ValueType min, ValueType max, bool /*merge_by_increasing_value*/)
            : MinMaxFeasibilityFunction(make_node_bounds(min, max)) {}

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
        /// @c BudgetExtensionFunction), or read it back through @ref bounds.
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
            cache_merge_bounds();
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

        /// @brief The merge test, chosen by the paired extension function's backward kind.
        ///
        ///  - @c Threshold: the backward value is a ceiling, so @c DominanceOrder
        ///    (`forward <= ceiling`); @c Unspecified if any minimum is non-zero, since a floor is
        ///    never checked on the backward side.
        ///  - @c Accumulate: the backward value is the suffix's consumption, so @c Custom
        ///    (`forward + backward <= capacity`, see @ref can_be_merged), but only for a uniform
        ///    window with a zero minimum. Otherwise the sum cannot be exact, so @c Unspecified.
        ///  - Otherwise (including unpaired): @c Unspecified, so bidirectional refuses.
        ///
        /// @return The merge rule implied by the paired extension function.
        [[nodiscard]] MergeRule merge_rule() const override {
            switch (this->backward_kind_) {
                case BackwardKind::Threshold:
                    return has_floor_ ? MergeRule::Unspecified : MergeRule::DominanceOrder;
                case BackwardKind::Accumulate:
                    return accumulate_merge_is_exact_ ? MergeRule::Custom : MergeRule::Unspecified;
                default:
                    return MergeRule::Unspecified;
            }
        }

        /// @brief The @c Accumulate merge test: `forward + backward` must fit under the cap.
        ///
        /// Only declared for a uniform `[0, max]` window (see @ref merge_rule), where the sum is
        /// the merged path's largest value and the test is exact. Assumes a cumulative (never
        /// decreasing, unclamped) extension.
        ///
        /// @param resource      The forward label's resource at the merge node.
        /// @param back_resource The backward label's resource at the merge node.
        /// @return `true` if the two consumptions together fit under the cap.
        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            if (this->backward_kind_ != BackwardKind::Accumulate) {
                throw std::logic_error(
                    "MinMaxFeasibilityFunction::can_be_merged is the accumulating form's body; a "
                    "threshold pairing merges through MergeRule::DominanceOrder and an unpaired "
                    "function declares MergeRule::Unspecified");
            }
            return static_cast<ValueType>(resource.get_value() + back_resource.get_value()) <=
                   bounds_->default_upper();
        }

        /// @brief Where a backward label starts, which follows the paired extension.
        ///
        /// A @c Threshold backward value is a ceiling, so it starts at this node's maximum. An
        /// @c Accumulate one is the suffix's consumption, so it starts at the empty suffix, the
        /// type default.
        ///
        /// @return This node's maximum, or @c std::nullopt under an accumulating extension.
        [[nodiscard]] auto back_seed_value() const -> std::optional<ResourceType> override {
            if (this->backward_kind_ == BackwardKind::Accumulate) {
                return std::nullopt;
            }
            ResourceType seed;
            seed.set_value(max_);
            return seed;
        }

        /// @brief This node's minimum, which @c is_back_feasible (the forward test) applies.
        ///
        /// @param node_id Index of the node.
        /// @return The node's minimum.
        [[nodiscard]] auto back_floor_at(size_t node_id) const
            -> std::optional<ResourceType> override {
            ResourceType floor;
            floor.set_value(bounds_->lower(node_id));
            return floor;
        }

        /// @brief The accumulating merge test adds the two halves, which bounds the path's
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

        /// @brief Whether `forward + backward <= max` is exact: the window is uniform with a zero
        ///        minimum. See @ref merge_rule.
        bool accumulate_merge_is_exact_ = true;

        /// @brief Whether any minimum, default or per node, is non-zero. See @ref merge_rule.
        bool has_floor_ = false;

        /// @brief Computes @ref has_floor_ and @ref accumulate_merge_is_exact_.
        void cache_merge_bounds() {
            has_floor_ = bounds_->default_lower() != ValueType{};
            // A non-zero minimum offsets the backward seed, and a per-node window bounds what the
            // path has consumed up to a node, which a backward label cannot know.
            accumulate_merge_is_exact_ = bounds_->is_uniform() && !has_floor_;
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

// No BackSeedEndOf specialisation: the seed end depends on the paired extension (a ceiling under a
// threshold, none under an accumulation), so the type keeps `Unknown`. Declaring `Ceiling` would
// wrongly reject the accumulating pairing, which is valid.

}  // namespace rcspp
