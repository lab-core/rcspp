// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <queue>
#include <utility>
#include <vector>

#include "rcspp/algorithm/dominance_algorithm.hpp"
#include "rcspp/preprocessor/bellman_ford_algorithm.hpp"
#include "rcspp/resource/resource_traits.hpp"

namespace rcspp {

/// @brief Label-correcting dominance algorithm with A*-style priority ordering.
///
/// Identical to @ref SimpleDominanceAlgorithm except the frontier is a min-heap ordered by
/// @f$ f = g + h @f$, where @f$ g @f$ is the label's current (reduced) cost and @f$ h(n) @f$ is a
/// per-node lower bound on the remaining reduced cost from @p n to any sink, computed once via a
/// backward Bellman–Ford pass over the same cost slot the labeling uses (see @ref initialize).
///
/// Expanding the smallest-@f$ f @f$ labels first typically extends far fewer labels than FIFO
/// ordering when arc costs are heterogeneous. Two things are worth being precise about:
///
///  - This is a *label-correcting* search: the whole frontier is processed (it does not stop at
///    the first sink reached), so a FULL search returns the optimal solution for ANY @f$ h @f$ —
///    the heuristic only changes the *order* of expansion, not which labels exist.
///  - @f$ h @f$ matters for correctness only together with label-dropping truncation (a per-node
///    extension cap, memory-pressure pruning, or @ref AlgorithmBaseParams::stop_after_X_solutions):
///    those keep the lowest-@f$ f @f$ labels, so an *admissible* @f$ h @f$ (a true lower bound)
///    keeps the most promising labels and the truncated result stays optimal far more often,
///    whereas an over-estimating @f$ h @f$ could drop the optimal path.
///
/// If the reduced-cost relaxation contains a negative-cost cycle there is no finite lower bound,
/// so the heuristic is disabled (@f$ h \equiv 0 @f$) and the search behaves like the non-A*
/// dominance algorithms (ordered by current reduced cost); see @ref initialize for why arc.cost
/// must not be used in that case.
///
/// @tparam ResourceType       Composed resource type (must satisfy ResourceTypeConcept).
/// @tparam LabelContainerType Non-dominated label container (default: LabelList).
/// @tparam CostResourceType   Numerical resource whose component value gives arc cost for
///                            the heuristic Bellman–Ford.  When it is not present in
///                            @p ResourceType, the algorithm falls back to @p arc.cost.
///                            Injected by AStarAlgoEntry so it matches the labeling cost.
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>,
          typename CostResourceType = RealResource>
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
              unprocessed_labels_{LabelFValueComparator{&h_to_sink_}} {}

        ~AStarDominanceAlgorithm() override = default;

    private:
        // ─── Comparator ──────────────────────────────────────────────────────────

        /// @brief Min-heap comparator: lower f-value = higher priority.
        ///
        /// Stores a pointer to the outer object's @ref h_to_sink_ vector so that
        /// the comparator always reflects the current heuristic values without
        /// copying the vector.
        struct LabelFValueComparator {
                const std::vector<double>* h;

                bool operator()(const LabelIteratorPair<ResourceType>& a,
                                const LabelIteratorPair<ResourceType>& b) const {
                    double fa = a.first->get_cost() + h->at(a.first->get_end_node()->pos());
                    double fb = b.first->get_cost() + h->at(b.first->get_end_node()->pos());
                    // std::priority_queue is a max-heap; invert comparison for min-heap.
                    return fa > fb;
                }
        };

        using PriorityQueue = std::priority_queue<LabelIteratorPair<ResourceType>,
                                                  std::vector<LabelIteratorPair<ResourceType>>,
                                                  LabelFValueComparator>;

        // ─── Initialization ───────────────────────────────────────────────────

        /// @brief Compute the per-node heuristic @ref h_to_sink_ and reset the per-node counters.
        ///
        /// The labeling cost @f$ g @f$ (Label::get_cost()) is the *reduced* cost, so for
        /// @f$ f = g + h @f$ to be an admissible A* heuristic @f$ h(n) @f$ must lower-bound the
        /// remaining REDUCED cost from @p n to a sink. We obtain that bound with a backward
        /// Bellman–Ford over the SAME cost slot the labeling uses
        /// (@p params_.heuristic_cost_index, the reduced cost): dropping the resource constraints
        /// can only lower a path's cost, so the relaxed shortest reduced-cost-to-sink is a valid
        /// lower bound on the true RCSPP cost-to-go. (When @p CostResourceType is not part of
        /// @p ResourceType, @f$ g @f$ is simply @p arc.cost and the arc-cost Bellman–Ford gives the
        /// matching bound.)
        ///
        /// Unreachable nodes get @f$ +\infty @f$; a negative-cost cycle disables the heuristic
        /// (@f$ h \equiv 0 @f$) — see the catch block.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);

            number_of_extended_labels_per_node_.assign(graph->get_number_of_nodes(), 0);
            // Default to h == 0 ("no heuristic"); overwritten below when a valid bound exists.
            h_to_sink_.assign(graph->get_number_of_nodes(), 0.0);

            try {
                // Backward Bellman–Ford from the sinks over the labeling cost slot. Throws on a
                // negative-cost cycle (possible with reduced costs, never with arc.cost alone).
                Distance dist;
                if constexpr (is_cost_in_composition_v<CostResourceType, ResourceType>) {
                    dist = BellmanFordAlgorithm::solve<CostResourceType>(
                        *graph,
                        graph->get_sink_node_ids(),
                        this->params_.heuristic_cost_index,
                        /*forward=*/false);
                } else {
                    dist = BellmanFordAlgorithm::solve(*graph,
                                                       graph->get_sink_node_ids(),
                                                       /*forward=*/false);
                }
                for (size_t node_id : graph->get_node_ids()) {
                    const auto* node = graph->get_node(node_id);
                    auto it = dist.find(node_id);
                    // Unreachable nodes -> +inf so their labels sort to the back of the heap.
                    h_to_sink_[node->pos()] =
                        (it != dist.end()) ? it->second : std::numeric_limits<double>::infinity();
                }
            } catch (const std::runtime_error&) {
                // The reduced-cost relaxation has a negative-cost cycle, so the shortest
                // reduced-cost-to-sink is -inf: there is no finite lower bound to use as h.
                //
                // Disable the heuristic (h == 0, already set by the assign() above) rather than
                // seeding it from arc.cost. arc.cost is the ORIGINAL, non-negative arc weight — a
                // different quantity from the reduced cost carried in g. Per arc, reduced cost <=
                // original cost (the duals are non-negative) and is frequently negative, so a sum
                // of arc.cost OVER-estimates the remaining reduced cost. An over-estimating h is
                // NOT admissible: with per-node truncation or memory-pressure pruning (which retain
                // the lowest-f labels) it can discard the labels lying on the true optimal path and
                // then return a suboptimal solution while still reporting
                // AlgorithmStatus::COMPLETE.
                //
                // With h == 0, A* is an ordinary reduced-cost-ordered label-correcting search — the
                // same ordering the non-A* dominance algorithms use. A full (untruncated) search is
                // still exact; under truncation it prunes by current reduced cost (a sensible
                // criterion) rather than by an unrelated original-cost metric.
                LOG_DEBUG(
                    "AStarDominanceAlgorithm: reduced-cost relaxation has a negative-cost cycle; "
                    "disabling the A* heuristic (h = 0) for this solve.\n");
            }

            // Rebuild priority queues with the fresh comparator.
            LabelFValueComparator cmp{&h_to_sink_};
            unprocessed_labels_ = PriorityQueue(cmp);
            unprocessed_truncated_labels_.clear();
        }

        // ─── Frontier management ──────────────────────────────────────────────

        LabelIteratorPair<ResourceType> next_label_iterator() override {
            LabelIteratorPair<ResourceType> label_iterator_pair;
            while (!unprocessed_labels_.empty()) {
                label_iterator_pair = unprocessed_labels_.top();
                unprocessed_labels_.pop();

                if (label_iterator_pair.first->dominated) {
                    // release_with_ref_count (not release_label): a dequeued label still pins
                    // the predecessor it was extended from; decrement its ref_count to avoid leak.
                    this->label_pool_.release_with_ref_count(label_iterator_pair.first);
                } else {
                    size_t& num_extended = number_of_extended_labels_per_node_.at(
                        label_iterator_pair.first->get_end_node()->pos());
                    if (num_extended < this->effective_max_labels_per_node_) {
                        ++num_extended;
                        break;
                    }
                    unprocessed_truncated_labels_.push_back(label_iterator_pair);
                }
            }
            return label_iterator_pair;
        }

        [[nodiscard]] size_t number_of_labels() const override {
            return unprocessed_labels_.size();
        }

        void add_new_unprocessed_label(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) override {
            unprocessed_labels_.push(label_iterator_pair);
        }

        // ─── Multi-phase support ──────────────────────────────────────────────

        /// @brief Restore truncated labels into the main heap for the next phase.
        void prepareNextPhase() override {
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
            this->effective_max_labels_per_node_ = limit;

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
            const auto& h = h_to_sink_;
            std::ranges::sort(flat, [&h](const auto& a, const auto& b) {
                if (a.first->dominated != b.first->dominated) {
                    return !a.first->dominated;
                }
                double fa = a.first->get_cost() + h[a.first->get_end_node()->pos()];
                double fb = b.first->get_cost() + h[b.first->get_end_node()->pos()];
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
                PriorityQueue(flat.begin(), flat.end(), LabelFValueComparator{&h_to_sink_});
        }

        // ─── Cleanup ──────────────────────────────────────────────────────────

        /// @brief Release label memory and clear all unprocessed label containers.
        void release_label_memory() override {
            DominanceAlgorithm<ResourceType, LabelContainerType>::release_label_memory();
            // Rebuild empty queues (pool has already freed the label objects).
            unprocessed_labels_ = PriorityQueue(LabelFValueComparator{&h_to_sink_});
            unprocessed_truncated_labels_.clear();
            std::ranges::fill(number_of_extended_labels_per_node_, 0);
        }

        // ─── Members ──────────────────────────────────────────────────────────

        /// @brief Admissible per-node lower bounds on the cost to the nearest sink.
        ///
        /// Indexed by node position (@ref Node::pos()).  Computed once per
        /// @ref initialize() call via a backward Bellman–Ford pass.
        std::vector<double> h_to_sink_;

        /// @brief Min-heap of active (non-truncated) labels ordered by f = g + h.
        PriorityQueue unprocessed_labels_;

        /// @brief Labels deferred due to per-node extension cap; restored each phase.
        std::vector<LabelIteratorPair<ResourceType>> unprocessed_truncated_labels_;

        /// @brief Count of labels extended per node in the current phase.
        std::vector<size_t> number_of_extended_labels_per_node_;
};

/// @brief Presents AStarDominanceAlgorithm<RT, LC, CostRC> as a 2-param template.
///
/// Required because C++ template template parameters must match exactly in arity
/// (P0522 matching is not reliably supported).  Use AStarAlgoBound<CostRC>::Algo
/// wherever a @c template<typename,typename> class argument is expected.
template <typename CostRC>
struct AStarAlgoBound {
        template <typename RT, typename LC>
        class Algo : public AStarDominanceAlgorithm<RT, LC, CostRC> {
                using AStarDominanceAlgorithm<RT, LC, CostRC>::AStarDominanceAlgorithm;
        };
};

}  // namespace rcspp
