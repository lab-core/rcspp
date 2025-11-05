// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/pushing_dominance_algorithm_iterators.hpp"

namespace rcspp {
template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class PullingDominanceAlgorithmIterators : public PushingDominanceAlgorithmIterators<ResourceType> {
    public:
        PullingDominanceAlgorithmIterators(ResourceFactory<ResourceType>* resource_factory,
                                           const Graph<ResourceType>& graph, bool use_pool = true)
            : PushingDominanceAlgorithmIterators<ResourceType>(resource_factory, graph, use_pool) {
            remove_current_label_ = false;
            current_unprocessed_label_iterator_ = this->current_unprocessed_labels_.begin();
        }

        ~PullingDominanceAlgorithmIterators() override = default;

    protected:
        void expand(Label<ResourceType>* label_ptr) override {
            // just store the label, as expansion is done when pulling
            assert(label_ptr->get_end_node()->id == this->current_unprocessed_node_id);
            this->expanded_labels_by_node_id_[this->current_unprocessed_node_id].push_back(
                label_ptr);
            // flag to not remove the current label in next_label_iterator
            remove_current_label_ = false;
        }

        std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>
        next_label_iterator() override {
            // remove the current label if needed, and move the iterator forward
            // TODO(Antoine): not remove if sink node? this->graph_.is_sink && !label.dominated
            if (remove_current_label_) {
                current_unprocessed_label_iterator_ =
                    this->current_unprocessed_labels_.erase(current_unprocessed_label_iterator_);
                --this->num_unprocessed_labels;
            } else {
                ++current_unprocessed_label_iterator_;
            }
            remove_current_label_ = true;  // by default, we remove the current label

            // if reach the end, move to next node as well as the iterator to the beginning
            if (current_unprocessed_label_iterator_ == this->current_unprocessed_labels_.end()) {
                // don't do anything on first iteration (i.e. num_loops_ == -1)
                if (this->num_loops_ >= 0) {
                    this->unprocessed_labels_by_node_id_[this->current_unprocessed_node_id] =
                        this->current_unprocessed_labels_;  // save unprocessed labels for the
                                                            // current node
                }

                // pull to the new node
                pull_new_unprocessed_labels();

                // reset the iterator to the beginning
                current_unprocessed_label_iterator_ = this->current_unprocessed_labels_.begin();
            }

            // get the next label
            return *current_unprocessed_label_iterator_;
        }

        void pull_new_unprocessed_labels() {
            // if no more labels for the current node, move to the next node with labels
            this->current_unprocessed_labels_.clear();
            while (this->current_unprocessed_labels_.empty() && this->num_unprocessed_labels > 0) {
                // move to the next node
                ++this->current_unprocessed_node_id;
                if (this->current_unprocessed_node_id >=
                    this->unprocessed_labels_by_node_id_.size()) {
                    this->current_unprocessed_node_id = 0;
                    ++this->num_loops_;
                }

                // clear current labels before adding new ones once a loop has been performed
                // all unprocessed labels from the previous loop have been processed
                if (this->num_loops_ > 0) {
                    auto& labels_at_node =
                        this->unprocessed_labels_by_node_id_[this->current_unprocessed_node_id];
                    // mark all those labels as processed
                    this->num_unprocessed_labels -= labels_at_node.size();
                    labels_at_node.clear();
                }

                // pull labels for the current node
                pull_labels();

                // move labels for the current node
                this->current_unprocessed_labels_ =
                    this->unprocessed_labels_by_node_id_[this->current_unprocessed_node_id];
                this->unprocessed_labels_by_node_id_[this->current_unprocessed_node_id].clear();
            }
        }

        void pull_labels() {
            // in pulling, we do not expand to nodes
            this->total_full_expand_time_.start();

            const auto& current_node = this->graph_.get_node(this->current_unprocessed_node_id);

            for (auto arc_ptr : current_node.in_arcs) {
                // pull all the unprocessed labels from the origin node
                const auto& unprocessed_labels =
                    this->unprocessed_labels_by_node_id_.at(arc_ptr->origin->id);
                for (const auto& label_iterator_pair : unprocessed_labels) {
                    this->expand_label(label_iterator_pair.first, arc_ptr);
                }
            }

            this->total_full_expand_time_.stop();
        }

        bool remove_current_label_;
        std::list<LabelIteratorPair<ResourceType>>::iterator current_unprocessed_label_iterator_;
};
}  // namespace rcspp
