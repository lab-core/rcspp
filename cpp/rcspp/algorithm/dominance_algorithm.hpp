// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <list>
#include <type_traits>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

/// @brief The labeling loop, written once for both search directions.
///
/// Every forward algorithm derives from it with the default direction; a backward search uses it
/// with @ref BackwardDirection. Wherever the two directions differ, the loop asks @p Dir. The
/// templated helpers take their direction and container set as parameters, so a bidirectional
/// algorithm can drive a second container set through the same code.
///
/// @tparam ResourceType       The resource type carried by labels.
/// @tparam LabelContainerType The per-node non-dominated label container its params hold; a
///                            backward search keeps it rebound (@c detail::rebind_direction).
/// @tparam Dir                The direction policy: @ref ForwardDirection (the default) or
///                            @ref BackwardDirection.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename Dir = ForwardDirection>
    requires ResourceTypeConcept<ResourceType>
class DominanceAlgorithm : public Algorithm<ResourceType, LabelContainerType> {
        static_assert(DirectionPolicy<Dir, ResourceType>,
                      "Dir does not satisfy DirectionPolicy for this ResourceType");

    public:
        using resource_type = ResourceType;
        using label_container_type = LabelContainerType;

        /// @brief The per-node container this search keeps: @c LabelContainerType rebound to
        ///        @c Dir.
        using DirectedContainer = detail::rebind_direction_t<LabelContainerType, Dir>;

        DominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                           AlgorithmParams<LabelContainerType> params)
            : Algorithm<ResourceType, LabelContainerType>(resource_factory, std::move(params)) {}

    protected:
        /// @brief Release label memory and clear the non-dominated label containers.
        ///
        /// Overrides @ref Algorithm::release_label_memory() to also clear
        /// @ref non_dominated_labels_by_node_pos_ so that no dangling label
        /// pointers remain after the pool is freed.  Subclasses that own
        /// additional label containers (e.g. unprocessed queues) should
        /// override this further and call this base implementation.
        void release_label_memory() override {
            Algorithm<ResourceType, LabelContainerType>::release_label_memory();
            non_dominated_labels_by_node_pos_.clear();
        }

        void initialize_labels() override {
            // Release all labels from the previous run (including any pending_release ones)
            // so the pool is fully reset before we start fresh.
            this->label_pool_.release_all_labels();

            non_dominated_labels_by_node_pos_.clear();
            non_dominated_labels_by_node_pos_.reserve(this->graph_->get_number_of_nodes());
            for (size_t i = 0; i < this->graph_->get_number_of_nodes(); i++) {
                non_dominated_labels_by_node_pos_.emplace_back(
                    detail::rebind_direction<LabelContainerType, Dir>::convert(
                        this->params_.labels));
            }

            for (auto start_node_id : Dir::start_nodes(*this->graph_)) {
                auto* start_node = this->graph_->get_node(start_node_id);
                auto& label = this->label_pool_.get_next_label(start_node);
                // Only start labels need starting values; extension overwrites all others.
                Dir::apply_start(*this->graph_, start_node, label);

                auto& buckets = non_dominated_labels_by_node_pos_.at(start_node->pos());
                // it points to the newly inserted element
                auto label_it = buckets.add_label(&label);
                add_new_unprocessed_label(std::make_pair(&label, label_it));
            }
        }

        void main_loop() override {  // NOLINT
            size_t i = 0;
            while (this->number_of_labels() > 0 && !this->should_stop(i)) {
                // Periodic memory check (skip i == 0 to avoid cost on every first iteration).
                if (i > 0 && this->memory_limit_.effective_limit > 0 &&
                    i % this->params_.memory_check_interval == 0) {
                    if (this->memory_limit_.is_exceeded()) {
                        LOG_WARN("Memory limit (",
                                 this->memory_limit_.effective_limit / (1024ULL * 1024ULL),
                                 " MB) exceeded (current: ",
                                 MemoryInfo::process_bytes() / (1024ULL * 1024ULL),
                                 " MB). Stopping early.\n");
                        break;
                    }
                    if (this->memory_limit_.is_under_pressure()) {
                        LOG_INFO("Memory pressure: ",
                                 MemoryInfo::process_bytes() / (1024ULL * 1024ULL),
                                 " MB / ",
                                 this->memory_limit_.effective_limit / (1024ULL * 1024ULL),
                                 " MB. Trimming label queues.\n");
                        this->on_memory_pressure();
                    }
                }

                ++i;

                // next label to process
                auto label_iterator_pair = next_label_iterator();

                // no more label -> break (useful when pulling)
                if (label_iterator_pair.first == nullptr) {
                    break;
                }

                // label dominated -> continue to next one
                auto& label = *label_iterator_pair.first;
                if (label.dominated) {
                    this->label_pool_.release_with_ref_count(&label);
                    continue;
                }
                if (this->params_.prune_based_on_upper_bound_ &&
                    label.get_cost() >= this->best_cost_upper_bound_) {
                    remove_label(label_iterator_pair.second);
                    // Use release_with_ref_count (not release_label): this label was added to
                    // the non-dominated set, so it pins a predecessor whose ref_count must be
                    // decremented. Plain release_label would leak that predecessor.
                    this->label_pool_.release_with_ref_count(&label);
                    continue;
                }

                assert(label.get_end_node());

                // check if we can update the best label or extend
                if (Dir::is_terminal(label.get_end_node())) {
                    if (label.get_cost() < this->cost_upper_bound_) {
                        if (label.get_cost() < this->best_cost_upper_bound_) {
                            this->best_cost_upper_bound_ = label.get_cost();
                        }
                        if (this->params_.return_dominated_solutions) {
                            this->extract_solution(label);
                            if (this->solutions_.size() >= this->params_.stop_after_X_solutions) {
                                LOG_DEBUG("Stopping after ",
                                          this->solutions_.size(),
                                          " solutions.\n");
                                return;
                            }
                        }
                    }
                } else if (!std::isinf(label.get_cost())) {
                    this->total_full_extend_time_.start();
                    this->extend(&label);
                    this->total_full_extend_time_.stop();
                } else {
                    remove_label(label_iterator_pair.second);
                    this->label_pool_.release_with_ref_count(&label);
                }
            }
        }

        /// @brief The next label to process and its position in its node's container; a null
        ///        label when none is left. Each subclass decides the order.
        virtual LabelIteratorPair<ResourceType> next_label_iterator() = 0;

        /// @brief Extends @p label_ptr along every arc its direction walks from its node.
        virtual void extend(Label<ResourceType>* label_ptr) {
            const auto& current_node = label_ptr->get_end_node();
            for (auto arc_ptr : Dir::arcs(*this->graph_, current_node)) {
                extend_label(label_ptr, arc_ptr);
            }
        }

        /// @brief Extends @p label_ptr along @p arc_ptr into this search's own containers.
        virtual void extend_label(Label<ResourceType>* label_ptr,
                                  const Arc<ResourceType>* arc_ptr) {
            extend_label<Dir>(label_ptr, arc_ptr, non_dominated_labels_by_node_pos_);
        }

        /// @brief Extends @p label_ptr along @p arc_ptr into an explicit container set.
        ///
        /// @tparam Dir2      The direction to extend in.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param label_ptr  The label to extend.
        /// @param arc_ptr    The arc to extend along.
        /// @param containers The per-node containers to insert the result into.
        template <typename Dir2, typename Container>
        void extend_label(Label<ResourceType>* label_ptr, const Arc<ResourceType>* arc_ptr,
                          std::vector<Container>& containers) {
            auto* head_node = Dir2::head(*arc_ptr);

            // Skip the arc when the label can no longer reach the node it lands on. Both
            // directions call the same `is_reachable`, which is correct only when it depends on
            // the node id alone. A resource whose check reads the label's visited set must declare
            // `JoinRule::Unspecified` (as `ReachableFeasibilityFunction` does), so the pre-solve
            // checks refuse a bidirectional search on it.
            if (!label_ptr->is_reachable(head_node->id)) {
                return;
            }

            // Sources and sinks stay out of a path's interior: the graph refuses an arc that
            // enters a source or leaves a sink (Graph::add_arc), so no extension lands on a node
            // this direction starts from.
            assert(!Dir2::is_start_node(head_node) && "an arc enters a source or leaves a sink");

            auto& new_label = this->label_pool_.get_next_label(head_node);
            Dir2::extend(*label_ptr, *arc_ptr, &new_label);

            if (++this->num_extended_labels_ % 100000 == 0) {  // NOLINT
                print_labels();
                LOG_DEBUG("Processed ", this->num_extended_labels_, " labels so far...\n");
            }

            bool feasible = Dir2::feasible(new_label);
            if (feasible && dispatch_update_non_dominated_labels<Dir2>(new_label, containers)) {
                // Keep the label only if it is feasible and not dominated: store it at its node
                // and queue it for extension.
                auto new_label_it =
                    containers.at(new_label.get_end_node()->pos()).add_label(&new_label);
                add_new_unprocessed_label(std::make_pair(&new_label, new_label_it));
                // Pin predecessor: keep it alive until this label is released.
                new_label.set_prev_label(label_ptr);
            } else {
                if (!feasible) {
                    ++this->nb_infeasible_labels_;
                } else {
                    ++this->nb_dominated_labels_;
                }
                // new_label was never a predecessor; release immediately.
                this->label_pool_.release_label(&new_label);
            }
        }

        std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) override {
            return path_arc_ids<Dir>(label);
        }

        /// @brief The arc ids of the path @p label ends, in forward order.
        ///
        /// Reconstructed by following prev_label pointers, in O(hops): every accepted label
        /// stores a pointer to its predecessor, kept alive via @ref ref_count until this label is
        /// released. Forward, the in-arc chain is reversed; backward, the out-arc chain is already
        /// in forward order.
        ///
        /// @tparam Dir2 The direction @p label was extended in.
        template <typename Dir2>
        [[nodiscard]] std::vector<size_t> path_arc_ids(const Label<ResourceType>& label) const {
            std::vector<size_t> arc_ids;
            const Label<ResourceType>* cur = &label;
            if constexpr (Dir2::backward) {
                while (cur != nullptr && cur->get_out_arc() != nullptr) {
                    arc_ids.push_back(cur->get_out_arc()->id);
                    cur = cur->prev_label;
                }
            } else {
                while (cur != nullptr && cur->get_in_arc() != nullptr) {
                    arc_ids.push_back(cur->get_in_arc()->id);
                    cur = cur->prev_label;
                }
                std::ranges::reverse(arc_ids);
            }
            return arc_ids;
        }

        using Algorithm<ResourceType, LabelContainerType>::extract_solution;

        void extract_solution(const Label<ResourceType>& label) override {
            extract_path_solution<Dir>(label);
        }

        /// @brief Records the path @p label ends.
        ///
        /// Going backward, a label ends at a source, so the path's last node is the head of its
        /// last arc: the sink the chain started from.
        ///
        /// @tparam Dir2 The direction @p label was extended in.
        template <typename Dir2>
        void extract_path_solution(const Label<ResourceType>& label) {
            if constexpr (Dir2::backward) {
                auto arc_ids = path_arc_ids<Dir2>(label);
                if (arc_ids.empty()) {
                    return;
                }
                const size_t end_node_id = this->graph_->get_arc(arc_ids.back())->destination->id;
                this->extract_solution(label.get_cost(), std::move(arc_ids), end_node_id);
            } else {
                Algorithm<ResourceType, LabelContainerType>::extract_solution(label);
            }
        }

        /// @brief Updates @p containers with @p label, through the virtual overload when they are
        ///        this search's own set, so a subclass override runs.
        ///
        /// The backward set of a bidirectional search has another container type, so a forward
        /// override could not apply to it anyway.
        template <typename Dir2, typename Container>
        bool dispatch_update_non_dominated_labels(const Label<ResourceType>& label,
                                                  std::vector<Container>& containers) {
            if constexpr (std::is_same_v<Dir2, Dir> &&
                          std::is_same_v<Container, DirectedContainer>) {
                if (&containers == &non_dominated_labels_by_node_pos_) {
                    return update_non_dominated_labels(label);
                }
            }
            return update_non_dominated_labels<Dir2>(label, containers);
        }

        /// @brief Updates this search's own non-dominated set with @p label.
        virtual bool update_non_dominated_labels(const Label<ResourceType>& label) {
            return update_non_dominated_labels<Dir>(label, non_dominated_labels_by_node_pos_);
        }

        /// @brief Updates an explicit non-dominated set with @p label.
        ///
        /// @tparam Dir2      The direction whose dominance order applies.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param label      The candidate label.
        /// @param containers The per-node containers to test against and prune.
        /// @return @c true when @p label is not dominated and should be kept.
        template <typename Dir2, typename Container>
        bool update_non_dominated_labels(const Label<ResourceType>& label,
                                         std::vector<Container>& containers) {
            total_update_non_dom_time_.start();
            ++nb_update_non_dom_iter_;

            auto current_node_pos = label.get_end_node()->pos();
            auto& non_dominated_labels_list = containers.at(current_node_pos);

            // First, check if label is dominated by any existing non-dominated label
            bool label_dominated = non_dominated_labels_list.is_dominated(label);
            if (label_dominated) {
                total_update_non_dom_time_.stop();
                return false;
            }

            // Second, remove all existing labels that are dominated by label
            non_dominated_labels_list.remove_dominated_labels(label);

            total_update_non_dom_time_.stop();

            return true;
        }

        /// @brief Removes @p label_iterator from this search's own containers.
        virtual void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator) {
            remove_label<Dir>(label_iterator, non_dominated_labels_by_node_pos_);
        }

        /// @brief Removes @p label_iterator from an explicit container set.
        ///
        /// @tparam Dir2      The direction the containers belong to.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param label_iterator Iterator into the container holding the label.
        /// @param containers The per-node containers to erase from.
        template <typename Dir2, typename Container>
        void remove_label(const std::list<Label<ResourceType>*>::iterator& label_iterator,
                          std::vector<Container>& containers) {
            auto current_node_pos = (*label_iterator)->get_end_node()->pos();
            containers.at(current_node_pos).erase_label(label_iterator);
        }

        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_sinks() const override {
            return get_labels_at_terminals<Dir>(non_dominated_labels_by_node_pos_);
        }

        /// @brief Collects the labels at this direction's terminal nodes.
        ///
        /// @tparam Dir2      The direction whose terminals to sweep.
        /// @tparam Container The per-node non-dominated label container type.
        /// @param containers The per-node containers to read.
        /// @return Every label held at a terminal node of @c Dir2.
        template <typename Dir2, typename Container>
        [[nodiscard]] std::list<Label<ResourceType>*> get_labels_at_terminals(
            const std::vector<Container>& containers) const {
            std::list<Label<ResourceType>*> labels_at_terminals;
            for (auto terminal_node_id : Dir2::terminals(*this->graph_)) {
                auto node_pos = this->graph_->get_node(terminal_node_id)->pos();
                const auto& labels_at_current_terminal = containers.at(node_pos).get_labels();
                labels_at_terminals.insert(labels_at_terminals.end(),
                                           labels_at_current_terminal.begin(),
                                           labels_at_current_terminal.end());
            }

            return labels_at_terminals;
        }

        void print_labels() const override {
            if (LOG_TRACE_ACTIVE()) {
                LOG_TRACE("All non dominated labels by node:\n");
                for (size_t pos = 0; pos < non_dominated_labels_by_node_pos_.size(); pos++) {
                    LOG_TRACE("Node ", this->graph_->get_sorted_nodes().at(pos)->id, ":\n");
                    non_dominated_labels_by_node_pos_.at(pos).print_labels();
                }
            }
        }

        /// @brief Queues a newly stored label for extension, in the subclass's own order.
        virtual void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) = 0;

        /// @brief This search's non-dominated labels: one container per node, indexed by node
        ///        position.
        std::vector<DirectedContainer> non_dominated_labels_by_node_pos_;

        Timer total_extend_time_;
        Timer total_update_non_dom_time_;

        size_t nb_infeasible_labels_ = 0;
        size_t nb_update_non_dom_iter_ = 0;
        size_t nb_extend_iter_ = 0;
};

template <typename ResourceType>
struct NodeUnprocessedLabelsManager {
        void initialize_unprocessed_labels(size_t num_nodes) {
            if (unprocessed_labels_by_node_pos_.empty()) {
                for (size_t i = 0; i < num_nodes; i++) {
                    unprocessed_labels_by_node_pos_.push_back(
                        std::list<LabelIteratorPair<ResourceType>>());
                    truncated_unprocessed_labels_by_node_pos_.push_back(
                        std::list<LabelIteratorPair<ResourceType>>());
                }
            }
            // save unprocessed labels for the current node
            unprocessed_labels_by_node_pos_.at(current_unprocessed_node_pos_)
                .splice(unprocessed_labels_by_node_pos_.at(current_unprocessed_node_pos_).end(),
                        current_unprocessed_labels_);
            // restart the loop at the beginning
            current_unprocessed_node_pos_ = 0;
            this->current_unprocessed_labels_ = std::move(unprocessed_labels_by_node_pos_.at(0));
        }

        void add_new_label(const LabelIteratorPair<ResourceType>& label_iterator_pair) {
            assert(check_number_of_unprocessed_labels());
            size_t pos = label_iterator_pair.first->get_end_node()->pos();
            if (pos == current_unprocessed_node_pos_) {
                current_unprocessed_labels_.push_back(label_iterator_pair);
            } else {
                unprocessed_labels_by_node_pos_.at(pos).push_back(label_iterator_pair);
            }
            ++num_unprocessed_labels_;
        }

        void resize_current_unprocessed_labels(size_t new_size,
                                               LabelPool<ResourceType>* label_pool = nullptr,
                                               bool sort = true) {
            assert(check_number_of_unprocessed_labels());
            resize_unprocessed_labels(&current_unprocessed_labels_, new_size, label_pool, sort);
        }

        void resize_unprocessed_labels(
            std::list<LabelIteratorPair<ResourceType>>* unprocessed_labels, size_t new_size,
            LabelPool<ResourceType>* label_pool, bool sort) {
            if (unprocessed_labels->size() <= new_size) {
                return;
            }
            size_t num_exceeding_labels = unprocessed_labels->size() - new_size;

            if (sort) {
                // sort labels by cost (ascending)
                unprocessed_labels->sort([](const LabelIteratorPair<ResourceType>& p1,
                                            const LabelIteratorPair<ResourceType>& p2) {
                    // either both dominated or both non-dominated
                    if (p1.first->dominated == p2.first->dominated) {
                        return p1.first->get_cost() < p2.first->get_cost();  // lower cost first
                    }
                    return !p1.first->dominated;  // non-dominated first
                });
            }

            // release the exceeding labels
            size_t i = 0;
            for (auto& p : *unprocessed_labels) {
                if (i++ >= new_size) {
                    if (p.first->dominated && label_pool) {
                        label_pool->release_with_ref_count(p.first);
                        p.first = nullptr;
                    } else {
                        store_truncated_unprocessed_label(p);
                    }
                }
            }

            // update unprocessed labels count and resize
            num_unprocessed_labels_ -= num_exceeding_labels;
            unprocessed_labels->resize(new_size);
            assert(check_number_of_unprocessed_labels());
        }

        void store_truncated_unprocessed_label(
            LabelIteratorPair<ResourceType> label_iterator_pair) {
            truncated_unprocessed_labels_by_node_pos_
                .at(label_iterator_pair.first->get_end_node()->pos())
                .push_back(std::move(label_iterator_pair));
        }

        void restore_truncated_unprocessed_labels() {
            size_t pos = 0;
            for (auto& truncated_labels : truncated_unprocessed_labels_by_node_pos_) {
                num_unprocessed_labels_ += truncated_labels.size();
                auto& unprocessed_labels = unprocessed_labels_by_node_pos_.at(pos++);
                unprocessed_labels.splice(unprocessed_labels.end(), truncated_labels);
            }
            // restart the loop at the beginning
            initialize_unprocessed_labels(unprocessed_labels_by_node_pos_.size());
            assert(check_number_of_unprocessed_labels());
        }

        /// @brief Trim all per-node unprocessed queues to at most max_per_node labels.
        ///
        /// Dominated excess labels are immediately recycled into @p pool.
        /// Non-dominated excess labels are stored in the truncated queue for a
        /// subsequent phase, consistent with resize_unprocessed_labels().
        ///
        /// @param max_per_node Maximum labels to retain per node (cheapest ones).
        /// @param pool  Label pool to recycle dominated labels into. May be nullptr.
        void trim_all_queues(size_t max_per_node, LabelPool<ResourceType>* pool) {
            resize_current_unprocessed_labels(max_per_node, pool);
            for (auto& labels_at_node : unprocessed_labels_by_node_pos_) {
                resize_unprocessed_labels(&labels_at_node, max_per_node, pool, /*sort=*/true);
            }
        }

        /// @brief Release and discard all labels currently in the truncated queue.
        ///
        /// For each truncated label: invokes @p remove_from_nondom (a callable with
        /// signature `void(const std::list<Label<ResourceType>*>::iterator&)`) to
        /// remove it from the non-dominated container, then recycles it into @p pool.
        /// Clears the truncated queues afterwards.
        ///
        /// Truncated labels are not counted in @ref num_unprocessed_labels_, so no
        /// counter update is needed.
        ///
        /// @param pool              Pool to recycle labels into.
        /// @param remove_from_nondom  Callable that removes a label from its node's
        ///                            non-dominated set given the list iterator.
        template <typename RemoveFn>
        void release_truncated_labels(LabelPool<ResourceType>* pool,
                                      RemoveFn&& remove_from_nondom) {
            for (auto& truncated_list : truncated_unprocessed_labels_by_node_pos_) {
                for (auto& [label_ptr, label_iter] : truncated_list) {
                    remove_from_nondom(label_iter);
                    // Truncated labels were non-dominated (added to the set), so they pin a
                    // predecessor and may themselves be pinned: release_with_ref_count keeps the
                    // ref_count chain balanced instead of leaking it.
                    pool->release_with_ref_count(label_ptr);
                }
                truncated_list.clear();
            }
        }

        /// @brief Clear all unprocessed and truncated queues.
        ///
        /// Does NOT release the Label objects (pool owns them).  Call this
        /// after the pool has been freed so no dangling pointers remain in
        /// the queues.
        void clear_all_queues() {
            current_unprocessed_labels_.clear();
            for (auto& labels : unprocessed_labels_by_node_pos_) {
                labels.clear();
            }
            for (auto& labels : truncated_unprocessed_labels_by_node_pos_) {
                labels.clear();
            }
            num_unprocessed_labels_ = 0;
            current_unprocessed_node_pos_ = 0;
        }

        [[nodiscard]] bool check_number_of_unprocessed_labels() const {
            size_t total_labels = 0;
            for (const auto& labels_at_node : unprocessed_labels_by_node_pos_) {
                total_labels += labels_at_node.size();
            }
            total_labels += current_unprocessed_labels_.size();
            if (total_labels != num_unprocessed_labels_) {
                LOG_ERROR("Mismatch in number of unprocessed labels: counted ",
                          total_labels,
                          " vs stored ",
                          num_unprocessed_labels_,
                          "\n");
                return false;
            }
            return true;
        }

        size_t num_unprocessed_labels_ = 0;
        size_t current_unprocessed_node_pos_ = 0;
        size_t num_loops_ = 0;
        std::list<LabelIteratorPair<ResourceType>> current_unprocessed_labels_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>> unprocessed_labels_by_node_pos_;
        std::vector<std::list<LabelIteratorPair<ResourceType>>>
            truncated_unprocessed_labels_by_node_pos_;
};
}  // namespace rcspp
