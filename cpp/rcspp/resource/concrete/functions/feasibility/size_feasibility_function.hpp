// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
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
        /// **The union, not the sum.** The two halves can have collected the same element, and
        /// counts do not add when the sets overlap: a forward half holding `{1,2}` and a backward
        /// half holding `{1,2,3,4}` sum to 6 while the merged path holds `{1,2,3,4}` -- four
        /// elements. Summing refused splices the model permits, which is not the safe direction
        /// it looks like: it does not corrupt a bound, it makes `bidirectional` answer a stricter
        /// question than `simple` on the same model. `MergeContract` in
        /// `tests/cpp/test_equivalence_ng.hpp` pins both directions.
        ///
        /// **The tightest cap in the model, not this node's.** The merged path has to fit under
        /// the cap at every node it passes through *after* the join, and nothing else checks
        /// those: the forward half never got there, and the backward half only ever counted its
        /// own contribution, which is a different quantity from the merged count. Comparing
        /// against the join node's cap alone therefore accepted splices that are infeasible
        /// downstream -- measured on a four-node graph capped at 2 only at the sink, where the
        /// rule passed a route arriving there with 3.
        ///
        /// For a **cumulative** container -- one whose extension only adds, which is what a size
        /// cap is for -- the count is largest at the end of the path, so `|forward u backward|` is
        /// the count at the sink and bounds it everywhere in between. With a **uniform** cap this
        /// test is therefore exact. With per-node caps the tightest one is sound but conservative:
        /// a cap on a node the merged path never visits still gates the join. There is no sharper
        /// choice available here, because `can_be_merged` sees two values and not the suffix's
        /// nodes.
        ///
        /// Only the UPPER bound: the lower one cannot be checked mid-join, because the merged path
        /// only grows from here.
        ///
        /// @param resource      The forward label's resource at the merge node.
        /// @param back_resource The backward label's resource at the merge node.
        /// @return `true` if the combined element count fits under the upper bound.
        [[nodiscard]] auto can_be_merged(const ResourceType& resource,
                                         const ResourceType& back_resource) -> bool override {
            ResourceType merged;
            merged.set_value(resource.get_union(back_resource.get_value()));
            return merged.size() <= tightest_max_size_;
        }

        /// @brief A container's *cardinality* cannot be stored as a threshold on the backward
        ///        label, so this is the only rule that needs its own body -- and only the UPPER
        ///        bound has one.
        ///
        /// The backward label holds its own half's elements rather than a complemented count, so
        /// there is nothing for @c DominanceOrder to compare -- hence a body of its own.
        ///
        /// **@c Unspecified when a lower bound is configured anywhere**, so a bidirectional solve
        /// refuses to start rather than losing paths. @ref can_be_merged already says it cannot
        /// check the lower bound, because the merged path only grows from the join. What that note
        /// does not say is that the lower bound is not merely *unchecked* at the join -- it is
        /// actively checked in the wrong place. `is_feasible` tests `size >= min_size_`, and
        /// `is_back_feasible` is inherited from @c FeasibilityFunction, so a backward label is
        /// asked whether the *suffix alone* already has enough elements. A prefix that satisfies
        /// the minimum cannot rescue it: the backward half is discarded before the join, and the
        /// route is gone while the solve reports COMPLETE.
        ///
        /// The two halves need opposite treatment -- the cap is suffix-safe, the floor is a
        /// whole-path property -- and a single `is_back_feasible` cannot give them that while the
        /// backward label stores a plain element set. So the pairing is refused instead. With the
        /// default `min_size` of 0, which is every use in this repository, nothing changes.
        ///
        /// @return @c MergeRule::Unspecified when any lower bound is non-zero, @c Custom otherwise.
        [[nodiscard]] MergeRule merge_rule() const override {
            return has_lower_bound_ ? MergeRule::Unspecified : MergeRule::Custom;
        }

        /// @brief Only with per-node caps, and then only because the tightest one is used.
        ///
        /// With a uniform cap `|forward u backward|` is the merged path's count at its last node
        /// and bounds it everywhere earlier, so the test in @ref can_be_merged is exact and a
        /// refusal can be trusted. With per-node caps it is compared against the smallest cap in
        /// the model -- sound, but a cap on a node the merged path never visits still gates the
        /// join. Asking the joiner to replay those refusals recovers what the conservatism costs;
        /// see @c FeasibilityFunction::merge_refusal_may_be_conservative.
        ///
        /// @return @c true when per-node caps were supplied.
        [[nodiscard]] bool merge_refusal_may_be_conservative() const override {
            return min_max_size_by_node_id_ != nullptr;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::pair<size_t, size_t>>> min_max_size_by_node_id_;

        size_t default_min_size_;
        size_t default_max_size_;
        size_t min_size_;
        size_t max_size_;

        /// @brief The smallest upper bound anywhere in the model, cached at construction.
        ///
        /// Read by @ref can_be_merged, which has to hold for every node the merged path visits
        /// after the join and cannot see which those are. Equal to @ref max_size_ at every node
        /// when no per-node overrides are given, which is the case the test is exact for.
        size_t tightest_max_size_ = 0;

        /// @brief Whether any node's window has a non-zero floor. See @ref merge_rule.
        ///
        /// Whole-function rather than per node, for the same reason @ref tightest_max_size_ is: the
        /// question the merge rule answers is about the model, and `merge_rule()` is cached per
        /// node resource at bind time, so a per-node answer would make one model's components
        /// disagree about their own rule.
        bool has_lower_bound_ = false;

        /// @brief Computes @ref tightest_max_size_ and @ref has_lower_bound_ from the defaults and
        ///        any per-node overrides.
        void cache_merge_bounds() {
            tightest_max_size_ = default_max_size_;
            has_lower_bound_ = default_min_size_ > 0;
            if (min_max_size_by_node_id_ == nullptr) {
                return;
            }
            for (const auto& [node_id, bounds] : *min_max_size_by_node_id_) {
                tightest_max_size_ = std::min(tightest_max_size_, bounds.second);
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
