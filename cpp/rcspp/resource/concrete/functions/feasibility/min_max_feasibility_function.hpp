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

namespace rcspp {

/// @brief Feasibility function that enforces a [min, max] bound on a scalar resource value,
///        with optional per-node override bounds.
///
/// At each graph node the active window [min_, max_] is either the global default or the
/// per-node override supplied at construction.  The resource is feasible when
/// `min_ <= resource.value <= max_`.
///
/// The bidirectional join test depends on the paired extension function's backward kind; see
/// @ref merge_rule.
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
            : default_min_(min), default_max_(max), min_(min), max_(max) {
            cache_merge_bounds();
        }

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
            : min_max_by_node_id_(
                  min_max_by_node_id.empty()
                      ? nullptr
                      : std::make_shared<const std::map<size_t, std::pair<ValueType, ValueType>>>(
                            std::move(min_max_by_node_id))),
              default_min_(default_min),
              default_max_(default_max),
              min_(default_min),
              max_(default_max) {
            cache_merge_bounds();
        }

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
                   default_max_;
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

        /// @brief This node's upper bound (override or default), used to clamp backward labels.
        ///
        /// @param node_id Index of the node.
        /// @return The node's maximum.
        [[nodiscard]] auto ceiling_at(size_t node_id) const
            -> std::optional<ResourceType> override {
            ResourceType ceiling;
            ceiling.set_value(bounds_at(node_id).second);
            return ceiling;
        }

        /// @brief This node's minimum, which @c is_back_feasible (the forward test) applies.
        ///
        /// @param node_id Index of the node.
        /// @return The node's minimum.
        [[nodiscard]] auto back_floor_at(size_t node_id) const
            -> std::optional<ResourceType> override {
            ResourceType floor;
            floor.set_value(bounds_at(node_id).first);
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
        std::shared_ptr<const std::map<size_t, std::pair<ValueType, ValueType>>>
            min_max_by_node_id_;
        ValueType default_min_{};
        ValueType default_max_{};
        ValueType min_;
        ValueType max_;

        /// @brief Whether `forward + backward <= max` is exact: the window is uniform with a zero
        ///        minimum. See @ref merge_rule.
        bool accumulate_merge_is_exact_ = true;

        /// @brief Whether any minimum, default or per node, is non-zero. See @ref merge_rule.
        bool has_floor_ = false;

        /// @brief Computes @ref has_floor_ and @ref accumulate_merge_is_exact_.
        void cache_merge_bounds() {
            has_floor_ = default_min_ != ValueType{};
            // A non-zero minimum offsets the backward seed, and a per-node window bounds what the
            // path has consumed up to a node, which a backward label cannot know.
            accumulate_merge_is_exact_ =
                min_max_by_node_id_ == nullptr && default_min_ == ValueType{};
            if (min_max_by_node_id_ == nullptr) {
                return;
            }
            for (const auto& [node_id, bounds] : *min_max_by_node_id_) {
                has_floor_ = has_floor_ || bounds.first != ValueType{};
            }
        }

        /// @brief The `{min, max}` window at @p node_id: its override, or the defaults.
        ///
        /// @param node_id Index of the node.
        /// @return The node's window.
        [[nodiscard]] std::pair<ValueType, ValueType> bounds_at(size_t node_id) const {
            if (min_max_by_node_id_ != nullptr) {
                auto it = min_max_by_node_id_->find(node_id);
                if (it != min_max_by_node_id_->end()) {
                    return it->second;
                }
            }
            return {default_min_, default_max_};
        }

        void preprocess(size_t node_id) override {
            if (min_max_by_node_id_ == nullptr) {
                return;
            }
            std::tie(min_, max_) = bounds_at(node_id);
        }
};

// No BackSeedEndOf specialisation: the seed end depends on the paired extension (a ceiling under a
// threshold, none under an accumulation), so the type keeps `Unknown`. Declaring `Ceiling` would
// wrongly reject the accumulating pairing, which is valid.

}  // namespace rcspp
