// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <unordered_map>
#include <vector>

#include "rcspp/label/label.hpp"

namespace rcspp {

/// @brief Flat list of non-dominated labels for a single node.
///
/// Used as the default label container in dominance algorithms.  Every
/// add / erase / dominance-check is O(N) in the number of stored labels.
template <class ResourceType>
class LabelList {
        using LabelPosition = std::list<Label<ResourceType>*>::iterator;

    public:
        explicit LabelList() = default;
        virtual ~LabelList() = default;

        /// @brief Returns an empty container with the same configuration.
        [[nodiscard]] LabelList copy() const { return LabelList(); }

        /// @brief Read-only access to the underlying label list.
        [[nodiscard]] const std::list<Label<ResourceType>*>& get_labels() const { return labels_; }

        /// @brief Appends @p label at the end and returns its iterator.
        virtual LabelPosition add_label(Label<ResourceType>* label) {
            return labels_.insert(labels_.end(), label);
        }

        /// @brief Removes the label at position @p pos.
        virtual void erase_label(const LabelPosition& pos) { labels_.erase(pos); }

        /// @brief Logs all stored labels at TRACE level.
        virtual void print_labels() const {
            if (LOG_TRACE_ACTIVE()) {
                for (auto label_ptr : labels_) {
                    LOG_TRACE("  ", label_ptr, ": ", label_ptr->get_resource().to_string(), "\n");
                }
            }
        }

        /// @brief Marks and removes all labels dominated by @p label.
        /// @return Number of labels removed.
        virtual size_t remove_dominated_labels(const Label<ResourceType>& label) {
            size_t removed = 0;
            for (auto non_dominated_label_it = labels_.begin();
                 non_dominated_label_it != labels_.end();) {
                if (&label != *non_dominated_label_it && label <= *(*non_dominated_label_it)) {
                    (*non_dominated_label_it)->dominated = true;
                    non_dominated_label_it = labels_.erase(non_dominated_label_it);
                    ++removed;
                } else {
                    ++non_dominated_label_it;
                }
            }
            return removed;
        }

        /// @brief Returns true if any stored label dominates @p label.
        [[nodiscard]] virtual bool is_dominated(const Label<ResourceType>& label) const {
            for (const auto non_dominated_label_ptr : labels_) {
                if (&label == non_dominated_label_ptr) {
                    continue;
                }
                if ((*non_dominated_label_ptr) <= label) {
                    return true;
                }
            }
            return false;
        }

    protected:
        std::list<Label<ResourceType>*> labels_;
};

/// @brief Bucket-partitioned label container with O(log B) lookup via binary search.
///
/// Labels are partitioned into buckets along the *bucket resource* axis; within
/// each bucket they are sorted by the *sort resource*.  This allows
/// - `remove_dominated_labels` to skip lower buckets entirely (a new label at
///   resource value v cannot dominate labels with bucket-resource < v - range),
/// - `is_dominated` to skip upper buckets (a stored label at resource value s
///   cannot dominate a new label with bucket-resource < s), and
/// - each bucket to apply an early-exit sort-resource check inside the inner loop.
///
/// ## Data structure choices
///
/// Bucket boundaries are stored in a `std::vector` for cache locality.  Binary
/// search (`find_first_not_after` / `find_first_before`) locates the relevant
/// bucket window in O(log B) instead of the O(B) linear scan a list would
/// require.
///
/// A hash map `begin_label_to_bucket_idx_` records, for each label that is
/// currently a bucket begin, its bucket index.  This makes `erase_label` O(1):
/// a single map lookup determines whether the erased label is a bucket boundary
/// and, if so, which bucket to update.  Without this map, `erase_label` would
/// need an O(B) `std::find_if` scan over all bucket begins.
///
/// ## Instrumentation
///
/// Both `remove_dominated_labels` and `is_dominated` accumulate counters for
/// total labels available and labels actually visited.  `print_labels` reports
/// both visit ratios so pruning efficiency can be compared symmetrically.
///
/// ## Adaptive bucket range
///
/// `range_buckets_` is fixed at construction.  `suggest_range(target_buckets)`
/// estimates the resource spread from the peak simultaneous bucket count and
/// returns a range calibrated for a chosen number of buckets.  Call it after a
/// warm-up phase and pass the result to the next `LabelBuckets` constructor to
/// improve bucket utilisation.
///
/// @tparam BucketResource  Resource type used to partition labels into buckets.
/// @tparam SortResource    Resource type used to sort labels within a bucket.
/// @tparam ResourceType    Full composite resource type of the labels.
template <typename BucketResource, typename SortResource, typename ResourceType>
class LabelBuckets : public LabelList<ResourceType> {
        using LabelPosition = std::list<Label<ResourceType>*>::iterator;
        using BucketIdx = size_t;

        template <class RType>
        struct Bucket {
                Bucket(const LabelPosition& pos, const RType* value, double range)
                    : begin(pos), end(std::next(pos)), begin_value(value), range(range) {}

                LabelPosition begin, end;
                const RType* begin_value;
                double range;

                /// @brief True when @p value is strictly before this bucket's range.
                [[nodiscard]] bool is_before_bucket(const RType& value) const {
                    return !begin_value->is_lower(value);
                }

                /// @brief True when @p value is strictly after this bucket's range.
                [[nodiscard]] bool is_after_bucket(const RType& value) const {
                    return begin_value->is_lower(value, -range);
                }

                /// @brief True when @p value falls within [begin_value, begin_value + range].
                [[nodiscard]] bool is_within_bucket(const RType& value) const {
                    return !is_before_bucket(value) && !is_after_bucket(value);
                }

                void update_begin(const LabelPosition& new_begin, const RType* new_min_value) {
                    begin = new_begin;
                    begin_value = new_min_value;
                }

                void update_end(const LabelPosition& new_end) { end = new_end; }
        };

    public:
        /// @brief Constructs a bucket container.
        ///
        /// @param range_buckets         Width of each bucket along the bucket resource axis.
        /// @param bucket_resource_index Index of the bucket resource in the composite resource.
        /// @param sort_resource_index   Index of the sort resource in the composite resource.
        LabelBuckets(size_t range_buckets, size_t bucket_resource_index, size_t sort_resource_index)
            : range_buckets_(range_buckets),
              bucket_resource_index_(bucket_resource_index),
              sort_resource_index_(sort_resource_index) {}

        /// @brief Returns an empty container with the same configuration.
        [[nodiscard]] LabelBuckets copy() const {
            return LabelBuckets(range_buckets_, bucket_resource_index_, sort_resource_index_);
        }

        /// @brief Inserts @p label into the appropriate bucket (creating one if needed).
        ///
        /// The correct bucket is located in O(log B) via binary search; the label is
        /// then placed at its sorted position within the bucket in O(labels_per_bucket).
        ///
        /// @return Iterator to the newly inserted label.
        LabelPosition add_label(Label<ResourceType>* label) override {
            const auto& lbr = get_bucket_resource(*label);

            // O(log B): skip all "after" buckets whose range lies entirely below lbr.
            BucketIdx idx = find_first_not_after(lbr);

            while (idx < buckets_.size()) {
                if (buckets_[idx].is_before_bucket(lbr)) {
                    // lbr precedes this bucket's range — open a new bucket before it.
                    auto pos = this->labels_.insert(buckets_[idx].begin, label);
                    insert_bucket(idx, pos, &lbr);
                    return pos;
                }
                // Within range: if the next bucket also claims lbr, prefer the later one.
                if (idx + 1 < buckets_.size() && !buckets_[idx + 1].is_before_bucket(lbr)) {
                    ++idx;
                    continue;
                }
                // Insert at the sort-resource-ordered position within this bucket.
                const auto& lsr = get_sort_resource(*label);
                auto it = buckets_[idx].begin;
                const auto bucket_end = buckets_[idx].end;
                while (it != bucket_end && get_sort_resource(**it) <= lsr) {
                    ++it;
                }
                auto pos = this->labels_.insert(it, label);
                if (it == buckets_[idx].begin) {
                    update_bucket_begin(idx, pos, nullptr, &lbr);
                }
                return pos;
            }

            // lbr lies after all existing buckets — open a new one at the end.
            auto pos = this->labels_.insert(this->labels_.end(), label);
            insert_bucket(buckets_.size(), pos, &lbr);
            return pos;
        }

        /// @brief Removes the label at @p pos in O(1).
        ///
        /// The `begin_label_to_bucket_idx_` map is consulted first: if the label is a
        /// bucket begin the bucket is updated or removed; otherwise only the label list
        /// is touched.
        void erase_label(const LabelPosition& pos) override {
            Label<ResourceType>* label = *pos;
            auto map_it = begin_label_to_bucket_idx_.find(label);

            auto it = this->labels_.erase(pos);  // pos is now invalid

            if (map_it == begin_label_to_bucket_idx_.end()) {
                return;  // Interior label: no bucket bookkeeping needed.
            }

            const BucketIdx idx = map_it->second;
            // Pass the captured pointer so helpers do not dereference the
            // now-invalid bucket begin iterator.
            if (it == buckets_[idx].end) {
                remove_bucket(idx, label);
            } else {
                update_bucket_begin(idx, it, label);
            }
        }

        /// @brief Marks and removes all labels dominated by @p label.
        ///
        /// Iterates upper buckets backward; stops as soon as a bucket is entirely
        /// "after" the new label (lower buckets cannot be dominated).  Within each
        /// bucket the sort-resource ordering provides an additional early exit.
        ///
        /// @return Number of labels removed.
        size_t remove_dominated_labels(const Label<ResourceType>& label) override {
            if (buckets_.empty()) {
                return 0;
            }

            num_rm_labels_ += this->labels_.size();
            size_t removed = 0;
            const auto& lbr = get_bucket_resource(label);
            const auto& lsr = get_sort_resource(label);

            // O(log B): first bucket index potentially containing dominated labels.
            const BucketIdx first_idx = find_first_not_after(lbr);
            if (first_idx >= buckets_.size()) {
                return 0;
            }

            BucketIdx idx = buckets_.size();
            while (idx > first_idx) {
                --idx;

                auto label_it = buckets_[idx].end;
                bool reached_begin = false;
                while (!reached_begin) {
                    ++num_rm_visited_;
                    --label_it;
                    reached_begin = (label_it == buckets_[idx].begin);
                    auto* current = *label_it;
                    if (&label != current && label <= *current) {
                        current->dominated = true;
                        // Capture the begin pointer BEFORE erasing: the erase
                        // invalidates the stored bucket begin iterator.
                        Label<ResourceType>* begin_before_erase = reached_begin ? current : nullptr;
                        label_it = this->labels_.erase(label_it);
                        ++removed;
                        if (reached_begin) {
                            if (label_it == buckets_[idx].end) {
                                remove_bucket(idx, begin_before_erase);
                                break;  // Bucket gone; continue outer loop.
                            }
                            update_bucket_begin(idx, label_it, begin_before_erase);
                        }
                    } else if (!(lsr <= get_sort_resource(*current))) {
                        // Sort-resource pruning: remaining labels cannot be dominated.
                        break;
                    }
                }
            }

            return removed;
        }

        /// @brief Returns true if any stored label dominates @p label.
        ///
        /// Iterates lower buckets forward; stops at the first bucket that starts
        /// above the new label (upper buckets cannot dominate it).
        [[nodiscard]] bool is_dominated(const Label<ResourceType>& label) const override {
            if (buckets_.empty()) {
                return false;
            }

            num_dom_labels_ += this->labels_.size();
            const auto& lbr = get_bucket_resource(label);
            const auto& lsr = get_sort_resource(label);

            // O(log B): stop before buckets whose begin_value already exceeds lbr.
            const BucketIdx end_idx = find_first_before(lbr);

            for (BucketIdx idx = 0; idx < end_idx; ++idx) {
                const auto& bucket = buckets_[idx];
                for (auto it = bucket.begin; it != bucket.end; ++it) {
                    ++num_dom_visited_;
                    if (&label == *it) {
                        continue;
                    }
                    if (**it <= label) {
                        return true;
                    }
                    if (!(get_sort_resource(**it) <= lsr)) {
                        break;
                    }
                }
            }

            return false;
        }

        /// @brief Logs label list and bucket-efficiency statistics at TRACE level.
        void print_labels() const override {
            LabelList<ResourceType>::print_labels();
            const double rm_ratio =
                num_rm_labels_ == 0 ? 0.0 : static_cast<double>(num_rm_visited_) / num_rm_labels_;
            const double dom_ratio = num_dom_labels_ == 0
                                         ? 0.0
                                         : static_cast<double>(num_dom_visited_) / num_dom_labels_;
            LOG_TRACE("remove_dominated visit ratio: ", rm_ratio, "\n");
            LOG_TRACE("is_dominated     visit ratio: ", dom_ratio, "\n");
        }

        /// @brief Suggests a bucket range for a desired number of buckets.
        ///
        /// The estimate is derived from the peak simultaneous bucket count observed
        /// during the solve, which approximates the resource spread.  Call after a
        /// warm-up phase and pass the result to the next `LabelBuckets` constructor:
        ///
        /// ```cpp
        /// size_t better_range = container.suggest_range(50);
        /// auto next = LabelBuckets<BR, SR, RT>(better_range, bi, si);
        /// ```
        ///
        /// @param target_buckets  Desired number of buckets in the next phase.
        /// @return Suggested range_buckets value (>= 1).
        [[nodiscard]] size_t suggest_range(size_t target_buckets) const {
            if (target_buckets == 0 || max_live_buckets_ == 0) {
                return range_buckets_;
            }
            const size_t estimated_span = max_live_buckets_ * range_buckets_;
            return std::max<size_t>(1, (estimated_span + target_buckets - 1) / target_buckets);
        }

    private:
        size_t range_buckets_;
        size_t bucket_resource_index_;
        size_t sort_resource_index_;
        std::vector<Bucket<Resource<BucketResource>>> buckets_;

        /// Maps each label that is currently a bucket begin to its bucket index.
        /// Maintained by insert_bucket / remove_bucket / update_bucket_begin so
        /// that erase_label can find the owning bucket in O(1).
        std::unordered_map<Label<ResourceType>*, size_t> begin_label_to_bucket_idx_;

        // remove_dominated_labels instrumentation.
        size_t num_rm_labels_{0};
        size_t num_rm_visited_{0};
        // is_dominated instrumentation (mutable: is_dominated is const).
        mutable size_t num_dom_labels_{0};
        mutable size_t num_dom_visited_{0};

        // Peak simultaneous bucket count; drives suggest_range().
        size_t max_live_buckets_{0};

        // ── Resource accessors ───────────────────────────────────────────────────

        [[nodiscard]] const Resource<BucketResource>& get_bucket_resource(
            const Label<ResourceType>& label) const {
            return get_resource<BucketResource>(label, bucket_resource_index_);
        }

        [[nodiscard]] const Resource<SortResource>& get_sort_resource(
            const Label<ResourceType>& label) const {
            return get_resource<SortResource>(label, sort_resource_index_);
        }

        template <class RType>
        [[nodiscard]] static const Resource<RType>& get_resource(const Label<ResourceType>& label,
                                                                 size_t resource_index) {
            return label.get_resource().template get_component<RType>(resource_index);
        }

        // ── Binary search helpers ────────────────────────────────────────────────

        /// @brief Returns the first bucket index where `!is_after_bucket(v)`.
        ///
        /// `is_after_bucket(v)` is monotonically decreasing across the sorted
        /// bucket vector (true for low-index, false for high-index), so a standard
        /// lower-bound search locates the transition in O(log B).
        [[nodiscard]] BucketIdx find_first_not_after(const Resource<BucketResource>& v) const {
            BucketIdx lo = 0;
            BucketIdx hi = buckets_.size();
            while (lo < hi) {
                const BucketIdx mid = lo + ((hi - lo) / 2);
                if (buckets_[mid].is_after_bucket(v)) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo;
        }

        /// @brief Returns the first bucket index where `is_before_bucket(v)`.
        ///
        /// `is_before_bucket(v)` is monotonically increasing (false for low-index,
        /// true for high-index), so a standard lower-bound search applies.
        [[nodiscard]] BucketIdx find_first_before(const Resource<BucketResource>& v) const {
            BucketIdx lo = 0;
            BucketIdx hi = buckets_.size();
            while (lo < hi) {
                const BucketIdx mid = lo + ((hi - lo) / 2);
                if (buckets_[mid].is_before_bucket(v)) {
                    hi = mid;
                } else {
                    lo = mid + 1;
                }
            }
            return lo;
        }

        // ── Bucket maintenance ───────────────────────────────────────────────────

        /// @brief Inserts a new bucket at @p idx, shifting all later bucket indices.
        ///
        /// The `begin_label_to_bucket_idx_` map is updated (O(B)) before the vector
        /// insert so that all stored indices remain correct.
        void insert_bucket(BucketIdx idx, const LabelPosition& begin,
                           const Resource<BucketResource>* begin_bucket_resource = nullptr) {
            if (idx > 0) {
                buckets_[idx - 1].update_end(begin);
            }
            if (begin_bucket_resource == nullptr) {
                begin_bucket_resource = &get_bucket_resource(**begin);
            }
            // Shift all map entries at index >= idx upward before the vector insert.
            for (auto& [lbl, bidx] : begin_label_to_bucket_idx_) {
                if (bidx >= idx) {
                    ++bidx;
                }
            }
            buckets_.emplace(buckets_.begin() + static_cast<std::ptrdiff_t>(idx),
                             begin,
                             begin_bucket_resource,
                             range_buckets_);
            begin_label_to_bucket_idx_[*begin] = idx;

            if (buckets_.size() > max_live_buckets_) {
                max_live_buckets_ = buckets_.size();
            }
        }

        /// @brief Removes the bucket at @p idx, shifting all later bucket indices down.
        ///
        /// @param erased_begin  The label that WAS the bucket begin; pass when the
        ///                      corresponding list erase has already invalidated the
        ///                      stored iterator.  Pass nullptr when the iterator is
        ///                      still valid.
        void remove_bucket(BucketIdx idx, Label<ResourceType>* erased_begin = nullptr) {
            Label<ResourceType>* begin_label =
                (erased_begin != nullptr) ? erased_begin : *buckets_[idx].begin;
            begin_label_to_bucket_idx_.erase(begin_label);
            if (idx > 0) {
                buckets_[idx - 1].update_end(buckets_[idx].end);
            }
            buckets_.erase(buckets_.begin() + static_cast<std::ptrdiff_t>(idx));
            for (auto& [lbl, bidx] : begin_label_to_bucket_idx_) {
                if (bidx > idx) {
                    --bidx;
                }
            }
        }

        /// @brief Advances the begin of bucket @p idx to @p new_begin.
        ///
        /// @param erased_begin          Old begin label when the old iterator has
        ///                              already been invalidated (see remove_bucket).
        ///                              Pass nullptr when the iterator is still valid.
        /// @param begin_bucket_resource Resource of @p new_begin; computed lazily if
        ///                              nullptr.
        void update_bucket_begin(BucketIdx idx, const LabelPosition& new_begin,
                                 Label<ResourceType>* erased_begin = nullptr,
                                 const Resource<BucketResource>* begin_bucket_resource = nullptr) {
            Label<ResourceType>* old_begin =
                (erased_begin != nullptr) ? erased_begin : *buckets_[idx].begin;
            begin_label_to_bucket_idx_.erase(old_begin);
            if (idx > 0) {
                buckets_[idx - 1].update_end(new_begin);
            }
            if (begin_bucket_resource == nullptr) {
                begin_bucket_resource = &get_bucket_resource(**new_begin);
            }
            buckets_[idx].update_begin(new_begin, begin_bucket_resource);
            begin_label_to_bucket_idx_[*new_begin] = idx;
        }
};
}  // namespace rcspp
