// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/label/label.hpp"

namespace rcspp {

template <class ResourceType>
class LabelList {
        using LabelPosition = std::list<Label<ResourceType>*>::iterator;

    public:
        explicit LabelList() = default;
        virtual ~LabelList() = default;

        [[nodiscard]] LabelList copy() const { return LabelList(); }

        [[nodiscard]] const std::list<Label<ResourceType>*>& get_labels() const { return labels_; }

        virtual LabelPosition add_label(Label<ResourceType>* label) {
            return labels_.insert(labels_.end(), label);
        }

        virtual void erase_label(const LabelPosition& pos) { labels_.erase(pos); }

        virtual void print_labels() const {
            if (LOG_TRACE_ACTIVE()) {
                for (auto label_ptr : labels_) {
                    LOG_TRACE("  ", label_ptr, ": ", label_ptr->get_resource().to_string(), "\n");
                }
            }
        }

        virtual size_t remove_dominated_labels(const Label<ResourceType>& label) {
            // Remove all dominated label
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

template <typename BucketResource, typename SortResource, typename ResourceType>
class LabelBuckets : public LabelList<ResourceType> {
        using LabelPosition = std::list<Label<ResourceType>*>::iterator;

        template <class RType>
        struct Bucket {
                Bucket(const LabelPosition& pos, const RType* value, double range)
                    : begin(pos), end(std::next(pos)), begin_value(value), range(range) {}

                LabelPosition begin, end;
                const RType* begin_value;
                double range;

                bool is_within_bucket(const RType& value) const {
                    // neither before, nor after the bucket
                    return !is_before_bucket(value) && !is_after_bucket(value);
                }

                bool is_before_bucket(const RType& value) const {
                    return !begin_value->is_lower(value);
                }

                bool is_after_bucket(const RType& value) const {
                    return begin_value->is_lower(value, -range);
                }

                void update_begin(const LabelPosition& new_begin, const RType* new_min_value) {
                    begin = new_begin;
                    begin_value = new_min_value;
                }

                void update_end(const LabelPosition& new_end) { end = new_end; }
        };

        using BucketPosition = std::list<Bucket<Resource<BucketResource>>>::iterator;

    public:
        LabelBuckets(size_t range_buckets, size_t bucket_resource_index, size_t sort_resource_index)
            : range_buckets_(range_buckets),
              bucket_resource_index_(bucket_resource_index),
              sort_resource_index_(sort_resource_index) {}

        LabelBuckets copy() const {
            return LabelBuckets(range_buckets_, bucket_resource_index_, sort_resource_index_);
        }

        LabelPosition add_label(Label<ResourceType>* label) override {
            const auto& label_bucket_resource = get_bucket_resource(*label);
            auto bit = buckets_.begin();
            while (bit != buckets_.end()) {
                auto& bucket = *bit;
                if (bucket.is_before_bucket(label_bucket_resource)) {
                    // Insert a new bucket before the current one and insert the label there
                    LabelPosition pos = this->labels_.insert(bucket.begin, label);
                    insert_bucket(bit, pos, &label_bucket_resource);
                    return pos;
                }
                if (bucket.is_within_bucket(label_bucket_resource)) {
                    // Insert the label in the correct position within the bucket based on the sort
                    // resource
                    const auto& label_sort_resource = get_sort_resource(*label);
                    auto it = bucket.begin;
                    while (it != bucket.end && get_sort_resource(**it) <= label_sort_resource) {
                        ++it;
                    }
                    // insert label at the right position in the list of labels
                    LabelPosition pos = this->labels_.insert(it, label);

                    // update begin if necessary
                    if (it == bucket.begin) {
                        update_bucket_begin(bit, pos, &label_bucket_resource);
                    }

                    return pos;
                }
                ++bit;
            }

            // Insert a new bucket at the end of the list and insert the label there
            LabelPosition pos = this->labels_.insert(this->labels_.end(), label);
            insert_bucket(buckets_.end(), pos, &label_bucket_resource);
            return pos;
        }

        void erase_label(const LabelPosition& pos) override {
            // find the bucket containing the erased label
            const auto& label_bucket_resource = get_bucket_resource(**pos);
            auto bit = std::find_if(buckets_.begin(), buckets_.end(), [&](const auto& bucket) {
                return bucket.is_within_bucket(label_bucket_resource);
            });
            if (bit == buckets_.end()) {
                throw std::runtime_error("Label not found in any bucket");
            }

            // erase the label from the list of labels
            auto it = this->labels_.erase(pos);

            // update the bucket begin if first element
            // remove the bucket if containing only the label
            auto& bucket = *bit;
            if (pos == bucket.begin) {
                // if contains only the label, remove the bucket
                if (it == bucket.end) {
                    remove_bucket(bit);
                } else {
                    update_bucket_begin(bit, it);
                }
            }
        }

        size_t remove_dominated_labels(const Label<ResourceType>& label) override {
            // if no bucket, no label, return 0
            if (buckets_.empty()) {
                return 0;
            }

            // Remove all dominated label (starting by upper buckets). Lower buckets cannot be
            // dominated
            num_labels_ += this->labels_.size();
            size_t removed = 0;
            const auto& label_bucket_resource = get_bucket_resource(label);
            const auto& label_sort_resource = get_sort_resource(label);
            auto bit = buckets_.end();
            while (bit != buckets_.begin()) {
                --bit;
                auto& bucket = *bit;
                // if the label is after the bucket, we can stop, as all the remaining lower buckets
                // are before the label and cannot be dominated
                if (bucket.is_after_bucket(label_bucket_resource)) {
                    break;
                }
                // check if the labels can be dominated in the bucket
                auto non_dominated_label_it = bucket.end;
                auto new_begin = bucket.end;
                bool reached_begin = false;
                while (!reached_begin) {
                    ++num_visited_labels_;
                    --non_dominated_label_it;
                    reached_begin = (non_dominated_label_it == bucket.begin);
                    if (&label != *non_dominated_label_it && label <= **non_dominated_label_it) {
                        // remove the dominated label
                        (*non_dominated_label_it)->dominated = true;
                        non_dominated_label_it = this->labels_.erase(non_dominated_label_it);
                        ++removed;
                    } else {
                        // if not dominated, check if we can stop by comparing the sort resource of
                        // the current label with the label to remove. If the current label sort
                        // resource does not dominate the label sort resource, we can stop as the
                        // following labels in the bucket are sorted by the sort resource and cannot
                        // be dominated.
                        if (!(label_sort_resource <= get_sort_resource(**non_dominated_label_it))) {
                            // not modified as breaking before reaching begin or erasing the bucket
                            new_begin = bucket.begin;
                            break;
                        }
                        new_begin = non_dominated_label_it;  // update begin flag of the bucket
                    }
                }

                // update or erase the bucket if necessary
                // remove empty bucket if either no labels anymore or the new end is at the end of
                // the previous bucket
                if (new_begin != bucket.begin) {
                    if (new_begin == bucket.end) {
                        // all labels in the bucket are dominated, we can remove the bucket
                        bit = remove_bucket(bit);
                    } else {
                        update_bucket_begin(bit, new_begin);
                    }
                }
            }

            return removed;
        }

        bool is_dominated(const Label<ResourceType>& label) const override {
            // if no bucket, no label, return false
            if (buckets_.empty()) {
                return false;
            }

            // Check all labels (starting by the lower buckets). Upper buckets cannot dominate
            const auto& label_bucket_resource = get_bucket_resource(label);
            const auto& label_sort_resource = get_sort_resource(label);
            for (const auto& bucket : buckets_) {
                // if the label is before the bucket, we can stop, as all the following buckets are
                // after the label and cannot dominate
                if (bucket.is_before_bucket(label_bucket_resource)) {
                    break;
                }
                // check if the labels can dominate in the bucket
                for (auto it = bucket.begin; it != bucket.end; ++it) {
                    if (&label == *it) {
                        continue;
                    }
                    if (**it <= label) {
                        return true;
                    }
                    // if the sort resource of the current label does not dominate the label
                    // sort resource, we can stop, as the following labels in the bucket are
                    // sorted by the sort resource and cannot dominate
                    if (!(get_sort_resource(**it) <= label_sort_resource)) {
                        break;
                    }
                }
            }

            return false;
        }

        void print_labels() const override {
            LabelList<ResourceType>::print_labels();
            LOG_DEBUG("Ratio of visits: ", num_visited_labels_ * 1.0 / num_labels_, "\n");
        }

    private:
        size_t range_buckets_;
        size_t bucket_resource_index_;
        size_t sort_resource_index_;
        std::list<Bucket<Resource<BucketResource>>> buckets_;

        size_t num_labels_{0};
        size_t num_visited_labels_{0};

        const Resource<BucketResource>& get_bucket_resource(
            const Label<ResourceType>& label) const {
            return get_resource<BucketResource>(label, bucket_resource_index_);
        }

        const Resource<SortResource>& get_sort_resource(const Label<ResourceType>& label) const {
            return get_resource<SortResource>(label, sort_resource_index_);
        }

        void insert_bucket(const BucketPosition& bit, const LabelPosition& begin,
                           const Resource<BucketResource>* begin_bucket_resource = nullptr) {
            // update previous end to the begin of the new bucket, as the current bucket will be
            // added
            if (bit != buckets_.begin()) {
                std::prev(bit)->update_end(begin);
            }
            // insert the new bucket and bucket resource if not provided
            if (begin_bucket_resource == nullptr) {
                begin_bucket_resource = &get_bucket_resource(**begin);
            }
            Bucket<Resource<BucketResource>> new_bucket(begin,
                                                        begin_bucket_resource,
                                                        range_buckets_);
            buckets_.insert(bit, new_bucket);
        }

        BucketPosition remove_bucket(const BucketPosition& bit) {
            // update previous end to the end of the current bucket, as the current bucket will be
            // removed
            if (bit != buckets_.begin()) {
                // the new end is the end of the removed bucket, as the following bucket will be
                // after the removed bucket
                std::prev(bit)->update_end(bit->end);
            }
            // erase the bucket and return the next bucket position
            return buckets_.erase(bit);
        }

        void update_bucket_begin(const BucketPosition& bit, const LabelPosition& new_begin,
                                 const Resource<BucketResource>* begin_bucket_resource = nullptr) {
            // update previous end to the new begin of the current bucket, as the current bucket
            // will be updated
            if (bit != buckets_.begin()) {
                std::prev(bit)->update_end(new_begin);
            }
            // update the current bucket begin and bucket resource if not provided
            if (begin_bucket_resource == nullptr) {
                begin_bucket_resource = &get_bucket_resource(**new_begin);
            }
            bit->update_begin(new_begin, begin_bucket_resource);
        }

        template <class RType>
        static const Resource<RType>& get_resource(const Label<ResourceType>& label,
                                                   size_t resource_index) {
            return label.get_resource().template get_resource_component<RType>(resource_index);
        }
};
}  // namespace rcspp
