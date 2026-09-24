// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that enforces a [min_size, max_size] bound on a container
///        resource's element count, with optional per-node overrides.
///
/// At each graph node the active size window is either the global default or the
/// per-node override supplied at construction.  The resource is feasible when
/// `min_size_ <= resource.size() <= max_size_`.
///
/// @tparam ResourceType The resource type that exposes a `size()` member returning the
///         number of elements currently stored.
template <typename ResourceType>
class SizeFeasibilityFunction
    : public Clonable<SizeFeasibilityFunction<ResourceType>, FeasibilityFunction<ResourceType>> {
    public:
        /// @brief Constructs the function with default size bounds and optional per-node
        ///        overrides.
        ///
        /// @param default_min_size Default minimum number of elements required at every node
        ///        that has no specific override.
        /// @param default_max_size Default maximum number of elements allowed at every node
        ///        that has no specific override.
        /// @param min_max_size_by_node_id Map from node id to a `{min_size, max_size}` pair
        ///        that overrides the defaults for that node.  May be empty.
        SizeFeasibilityFunction(
            size_t default_min_size, size_t default_max_size,
            std::map<size_t, std::pair<size_t, size_t>> min_max_size_by_node_id = {})
            : min_max_size_by_node_id_(
                  min_max_size_by_node_id.empty()
                      ? nullptr
                      : std::make_shared<const std::map<size_t, std::pair<size_t, size_t>>>(
                            std::move(min_max_size_by_node_id))),
              default_min_size_(default_min_size),
              default_max_size_(default_max_size),
              min_size_(default_min_size),
              max_size_(default_max_size) {
            cache_merge_bounds();
        }

        /// @brief Constructs the function from a per-node size map with optional global
        ///        fallback bounds.
        ///
        /// Nodes not present in the map fall back to `[default_min_size, default_max_size]`.
        /// The upper default is capped at `numeric_limits<size_t>::max() / 2` to prevent
        /// overflow when callers add increments.
        ///
        /// @param min_max_size_by_node_id Map from node id to a `{min_size, max_size}` pair.
        /// @param default_min_size Default minimum size for nodes not in the map.
        /// @param default_max_size Default maximum size for nodes not in the map.
        explicit SizeFeasibilityFunction(
            std::map<size_t, std::pair<size_t, size_t>> min_max_size_by_node_id,
            size_t default_min_size = 0,
            size_t default_max_size = std::numeric_limits<size_t>::max() / 2)  // prevent overflow
            : min_max_size_by_node_id_(
                  std::make_shared<const std::map<size_t, std::pair<size_t, size_t>>>(
                      std::move(min_max_size_by_node_id))),
              default_min_size_(default_min_size),
              default_max_size_(default_max_size),
              min_size_(default_min_size),
              max_size_(default_max_size) {
            cache_merge_bounds();
        }

        /// @brief Checks that the resource's size lies within the active [min_size_, max_size_]
        ///        window.
        ///
        /// @param resource The resource whose size is evaluated.
        /// @return `true` if `min_size_ <= resource.size() <= max_size_`.
        auto is_feasible(const ResourceType& resource) -> bool override {
            const size_t size = resource.size();
            return size >= min_size_ && size <= max_size_;
        }

        /// @brief Whether the merged path's element count can stay within the cap.
        ///
        /// Counts the union of the two sets, not the sum, since halves may share elements. Only
        /// declared for a uniform cap (see @ref merge_rule), where the union is the merged path's
        /// largest set and the test is exact. Only the upper bound is checked.
        ///
        /// @param resource      The forward label's resource at the merge node.
        /// @param back_resource The backward label's resource at the merge node.
        /// @return `true` if the combined element count fits under the upper bound.
        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            ResourceType merged;
            merged.set_value(resource.get_union(back_resource.get_value()));
            return merged.size() <= default_max_size_;
        }

        /// @brief @c Custom merge rule (see @ref can_be_merged), or @c Unspecified when a lower
        ///        bound or a per-node cap is configured anywhere.
        ///
        /// Both bound what the path has collected up to a node, which a backward label cannot
        /// know: it holds only what it collected from there to the sink. The inherited
        /// @c is_back_feasible would lose paths under a floor and accept infeasible ones under a
        /// per-node cap, so bidirectional solves refuse instead.
        ///
        /// @return @c MergeRule::Unspecified when any lower bound is non-zero or any node's cap
        ///         differs from the default, @c Custom otherwise.
        [[nodiscard]] MergeRule merge_rule() const override {
            return has_lower_bound_ || has_per_node_cap_ ? MergeRule::Unspecified
                                                         : MergeRule::Custom;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<size_t, size_t>>> min_max_size_by_node_id_;

        size_t default_min_size_;
        size_t default_max_size_;
        size_t min_size_;
        size_t max_size_;

        /// @brief Whether any node's cap differs from the default. Model-wide, like
        ///        @ref has_lower_bound_.
        bool has_per_node_cap_ = false;

        /// @brief Whether any node's window has a non-zero floor. Model-wide so that every
        ///        node's @ref merge_rule agrees.
        bool has_lower_bound_ = false;

        /// @brief Computes @ref has_per_node_cap_ and @ref has_lower_bound_ from the defaults and
        ///        any per-node overrides.
        void cache_merge_bounds() {
            has_per_node_cap_ = false;
            has_lower_bound_ = default_min_size_ > 0;
            if (min_max_size_by_node_id_ == nullptr) {
                return;
            }
            for (const auto& [node_id, bounds] : *min_max_size_by_node_id_) {
                has_per_node_cap_ = has_per_node_cap_ || bounds.second != default_max_size_;
                has_lower_bound_ = has_lower_bound_ || bounds.first > 0;
            }
        }

        void preprocess(size_t node_id) override {
            if (min_max_size_by_node_id_ == nullptr) {
                return;
            }
            auto it = min_max_size_by_node_id_->find(node_id);
            if (it != min_max_size_by_node_id_->end()) {
                min_size_ = it->second.first;
                max_size_ = it->second.second;
            } else {
                min_size_ = default_min_size_;
                max_size_ = default_max_size_;
            }
        }
};

/// A size bound is tested against the container, not seeded into a backward label.
template <typename R>
struct BackSeedEndOf<SizeFeasibilityFunction<R>> {
        static constexpr BackSeedEnd value = BackSeedEnd::Never;
};

}  // namespace rcspp
