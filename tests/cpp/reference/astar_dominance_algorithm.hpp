// Frozen copy of cpp/rcspp/algorithm/astar_dominance_algorithm.hpp at 9c80f174,
// for the parity tests. Do not edit.
// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/bounds.hpp"
#include "reference/dominance_algorithm.hpp"

namespace rcspp::reference {

/// @brief Label-correcting dominance algorithm with A*-style priority ordering.
///
/// Identical to @ref SimpleDominanceAlgorithm except the frontier is a min-heap ordered by
/// @f$ f = g + h @f$, where @f$ g @f$ is the label's cost and @f$ h(n) @f$ a lower bound on the
/// cost still to pay from @p n to a sink, computed once per solve (@ref
/// detail::completion_bounds).
///
/// Expanding the smallest-@f$ f @f$ labels first typically extends far fewer labels than FIFO
/// ordering when arc costs are heterogeneous. Two things are worth being precise about:
///
///  - This is a *label-correcting* search: the whole frontier is processed (it does not stop at
///    the first sink reached), so a FULL search returns the optimal solution for ANY @f$ h @f$ —
///    the heuristic only changes the *order* of expansion, not which labels exist.
///  - @f$ h @f$ matters for correctness only together with label-dropping truncation (a per-node
///    extension cap, memory-pressure pruning, or @ref AlgorithmBaseParams::max_solutions):
///    those keep the lowest-@f$ f @f$ labels, so an *admissible* @f$ h @f$ (a true lower bound)
///    keeps the most promising labels and the truncated result stays optimal far more often,
///    whereas an over-estimating @f$ h @f$ could drop the optimal path.
///
/// The bound is computed on the objective, the cost the labels carry, so it is a lower bound
/// whichever components the cost function reads. If the relaxation contains a negative-cost
/// cycle there is no finite bound, and the search orders by cost alone, as the non-A* dominance
/// algorithms do.
///
/// @tparam ResourceType       Composed resource type (must satisfy ResourceTypeConcept).
/// @tparam LabelContainerType Non-dominated label container (default: LabelList).
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
    requires ResourceTypeConcept<ResourceType>
class AStarDominanceAlgorithm : public DominanceAlgorithm<ResourceType, LabelContainerType> {
    public:
        /// @brief Construct with a resource factory and algorithm parameters.
        ///
        /// @param resource_factory  Factory that creates initial resources for source labels.
        /// @param params            Algorithm configuration (truncation, memory limits, etc.).
        AStarDominanceAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                                AlgorithmParams<LabelContainerType> params)
            : DominanceAlgorithm<ResourceType, LabelContainerType>(resource_factory,
                                                                   std::move(params)),
              unprocessed_labels_{LabelFValueComparator{&h_}} {}

        ~AStarDominanceAlgorithm() override = default;

    private:
        // ─── Comparator ──────────────────────────────────────────────────────────

        /// @brief Min-heap comparator: lower f-value = higher priority; among equal f-values,
        ///        the older label (smaller id) first.
        ///
        /// Stores a pointer to the outer object's @ref h_ so that the comparator always
        /// reflects the current bound without copying it.
        ///
        /// The id tie-break makes the order total. Without it, which of two labels with the same
        /// f-value comes out first depends on the standard library's heap algorithm (libc++'s
        /// differs from libstdc++'s), and so did the labels A* extended and, under a truncation,
        /// the solutions it returned. Label ids come from the pool's counter, so they are unique
        /// and follow creation order.
        struct LabelFValueComparator {
                const detail::NodeBound<ResourceType>* h;

                bool operator()(const LabelIteratorPair<ResourceType>& a,
                                const LabelIteratorPair<ResourceType>& b) const {
                    double fa = a.first->get_cost() + h->for_order(*a.first);
                    double fb = b.first->get_cost() + h->for_order(*b.first);
                    // std::priority_queue is a max-heap; invert comparison for min-heap.
                    if (fa != fb) {
                        return fa > fb;
                    }
                    return a.first->id > b.first->id;
                }
        };

        using PriorityQueue = std::priority_queue<LabelIteratorPair<ResourceType>,
                                                  std::vector<LabelIteratorPair<ResourceType>>,
                                                  LabelFValueComparator>;

        // ─── Initialization ───────────────────────────────────────────────────

        /// @brief Compute the bound @ref h_ and reset the per-node counters.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);

            number_of_extended_labels_per_node_.assign(graph->get_number_of_nodes(), 0);
            h_ = detail::completion_bounds<ForwardDirection>(*graph);

            // Rebuild priority queues with the fresh comparator.
            LabelFValueComparator cmp{&h_};
            unprocessed_labels_ = PriorityQueue(cmp);
            unprocessed_truncated_labels_.clear();
        }

        // ─── Frontier management ──────────────────────────────────────────────

        LabelIteratorPair<ResourceType> next_label_iterator() override {
            while (!unprocessed_labels_.empty()) {
                const LabelIteratorPair<ResourceType> label_iterator_pair =
                    unprocessed_labels_.top();
                unprocessed_labels_.pop();

                if (label_iterator_pair.first->dominated) {
                    // release_with_ref_count (not release_label): a dequeued label still pins
                    // the predecessor it was extended from; decrement its ref_count to avoid leak.
                    this->label_pool_.release_with_ref_count(label_iterator_pair.first);
                } else {
                    size_t& num_extended = number_of_extended_labels_per_node_.at(
                        label_iterator_pair.first->get_end_node()->pos());
                    if (num_extended < this->quota_) {
                        ++num_extended;
                        return label_iterator_pair;
                    }
                    unprocessed_truncated_labels_.push_back(label_iterator_pair);
                }
            }
            // Every label taken was released or stored for the next phase: none to process. The
            // last one must not be handed back, or the loop would release it a second time or
            // extend a label it stored aside.
            return {};
        }

        [[nodiscard]] size_t waiting_labels() const override { return unprocessed_labels_.size(); }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_.push(label_iterator_pair);
        }

        // ─── Multi-phase support ──────────────────────────────────────────────

        /// @brief Restore truncated labels into the main heap for the next phase.
        void prepare_next_phase() override {
            std::ranges::fill(number_of_extended_labels_per_node_, 0);
            for (const auto& pair : unprocessed_truncated_labels_) {
                unprocessed_labels_.push(pair);
            }
            unprocessed_truncated_labels_.clear();
        }

        // ─── Memory pressure ──────────────────────────────────────────────────

        /// @brief Trim the heap when memory pressure is detected.
        ///
        /// Drains the heap into a temporary vector, sorts by dominance status
        /// then ascending f-value, keeps the cheapest
        /// @ref AlgorithmBaseParams::memory_pressure_max_labels_per_node × num_nodes
        /// entries, recycles dominated excess labels, and stores non-dominated
        /// excess in @ref unprocessed_truncated_labels_ for the next phase.
        void on_memory_pressure() override {
            const size_t limit = this->params_.memory_pressure_max_labels_per_node;
            this->quota_ = std::min(this->quota_, limit);

            if (this->memory_pressure_triggered_) {
                for (auto& [label_ptr, label_iter] : unprocessed_truncated_labels_) {
                    this->remove_label(label_iter);
                    this->label_pool_.release_with_ref_count(label_ptr);
                }
                unprocessed_truncated_labels_.clear();
            }
            this->memory_pressure_triggered_ = true;

            const size_t max_total = limit * this->graph_->get_number_of_nodes();
            if (unprocessed_labels_.size() <= max_total) {
                return;
            }

            // Drain heap into a flat vector for sorting.
            std::vector<LabelIteratorPair<ResourceType>> flat;
            flat.reserve(unprocessed_labels_.size());
            while (!unprocessed_labels_.empty()) {
                flat.push_back(unprocessed_labels_.top());
                unprocessed_labels_.pop();
            }

            // Non-dominated labels first; among equal dominance, lowest f-value first.
            const auto& h = h_;
            std::ranges::sort(flat, [&h](const auto& a, const auto& b) {
                if (a.first->dominated != b.first->dominated) {
                    return !a.first->dominated;
                }
                double fa = a.first->get_cost() + h.for_order(*a.first);
                double fb = b.first->get_cost() + h.for_order(*b.first);
                return fa < fb;
            });

            // Recycle or defer excess labels.
            for (size_t i = max_total; i < flat.size(); ++i) {
                auto& p = flat[i];
                if (p.first->dominated) {
                    this->label_pool_.release_with_ref_count(p.first);
                } else {
                    unprocessed_truncated_labels_.push_back(p);
                }
            }
            flat.resize(max_total);

            // Rebuild heap from retained labels.
            unprocessed_labels_ =
                PriorityQueue(flat.begin(), flat.end(), LabelFValueComparator{&h_});
        }

        // ─── Cleanup ──────────────────────────────────────────────────────────

        /// @brief Release label memory and clear all unprocessed label containers.
        void release_label_memory() override {
            DominanceAlgorithm<ResourceType, LabelContainerType>::release_label_memory();
            // Rebuild empty queues (pool has already freed the label objects).
            unprocessed_labels_ = PriorityQueue(LabelFValueComparator{&h_});
            unprocessed_truncated_labels_.clear();
            std::ranges::fill(number_of_extended_labels_per_node_, 0);
        }

        // ─── Members ──────────────────────────────────────────────────────────

        /// @brief A lower bound on the cost from each node to a sink; computed once per
        ///        @ref initialize() call.
        detail::NodeBound<ResourceType> h_;

        /// @brief Min-heap of active (non-truncated) labels ordered by f = g + h.
        PriorityQueue unprocessed_labels_;

        /// @brief Labels deferred due to per-node extension cap; restored each phase.
        std::vector<LabelIteratorPair<ResourceType>> unprocessed_truncated_labels_;

        /// @brief Count of labels extended per node in the current phase.
        std::vector<size_t> number_of_extended_labels_per_node_;
};

}  // namespace rcspp::reference
