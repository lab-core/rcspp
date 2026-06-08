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
/// Identical to @ref SimpleDominanceAlgorithm except the frontier is managed
/// by a min-heap ordered by @f$ f = g + h @f$, where @f$ g @f$ is the label's
/// current cost and @f$ h @f$ is a per-node admissible lower bound on the
/// remaining cost to any sink, computed once via a backward Bellman–Ford pass.
///
/// Because @f$ h @f$ is admissible, optimality is preserved.  Labels with
/// the smallest estimated total cost are expanded first, which typically
/// reduces the total number of labels extended compared to FIFO ordering
/// when arc costs are heterogeneous.
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

        /// @brief Initialize the heuristic vector and per-node counters.
        ///
        /// Runs a backward Bellman–Ford from all sinks to fill @ref h_to_sink_.
        /// When @p CostResourceType is present in @p ResourceType, the arc weight
        /// is read from the resource extender at @p params_.heuristic_cost_index
        /// (the reduced cost), giving a tight admissible lower bound aligned with
        /// the labeling cost.  Otherwise falls back to @p arc.cost.
        void initialize(const Graph<ResourceType>* graph, double cost_upper_bound) override {
            Algorithm<ResourceType, LabelContainerType>::initialize(graph, cost_upper_bound);

            number_of_extended_labels_per_node_.assign(graph->get_number_of_nodes(), 0);

            // Backward Bellman-Ford from sinks using the same cost slot as the labeling algorithm.
            // Reduced costs can create negative-weight cycles (impossible with arc.cost alone),
            // so we fall back to the arc-cost overload when that happens.
            Distance dist;
            try {
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
            } catch (const std::runtime_error&) {
                // Negative-weight cycle in reduced costs: fall back to arc.cost (always positive).
                dist = BellmanFordAlgorithm::solve(*graph,
                                                   graph->get_sink_node_ids(),
                                                   /*forward=*/false);
            }
            h_to_sink_.resize(graph->get_number_of_nodes());
            for (size_t node_id : graph->get_node_ids()) {
                const auto* node = graph->get_node(node_id);
                auto it = dist.find(node_id);
                h_to_sink_[node->pos()] =
                    (it != dist.end()) ? it->second : std::numeric_limits<double>::infinity();
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
