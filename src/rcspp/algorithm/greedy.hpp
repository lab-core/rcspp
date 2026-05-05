// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <utility>

#include "rcspp/algorithm/backtracking_dive_algorithm.hpp"

namespace rcspp {

/**
 * @brief GreedyAlgorithm for Resource Constrained Shortest Path Problems (RCSPP).
 *
 * This algorithm attempts to extend the current path greedily, always choosing the next best label
 * to extend. If a greedy extension is not possible, the algorithm backtracks to previous labels and
 * explores alternative siblings. This approach combines greedy search with backtracking, allowing
 * it to efficiently find feasible solutions while avoiding exhaustive enumeration of all possible
 * paths.
 *
 * Use this algorithm when you want a fast, heuristic approach to RCSPP that can quickly find good
 * solutions, but may not guarantee optimality in all cases. It is particularly useful for large
 * graphs where full enumeration is computationally expensive, and a balance between speed and
 * solution quality is desired.
 *
 * The DFS path/sibling/backtrack mechanics live in @ref BacktrackingDiveAlgorithm; this class only
 * supplies the main loop. The default cost-ascending child selection is inherited unchanged.
 */
template <typename ResourceType, typename LabelContainerType = LabelList<ResourceType>>
class GreedyAlgorithm : public BacktrackingDiveAlgorithm<ResourceType, LabelContainerType> {
    public:
        GreedyAlgorithm(ResourceFactory<ResourceType>* resource_factory,
                        AlgorithmParams<LabelContainerType> params)
            : BacktrackingDiveAlgorithm<ResourceType, LabelContainerType>(resource_factory,
                                                                          std::move(params)) {}

    protected:
        void main_loop() override {
            size_t i = 0;
            while (this->number_of_labels() > 0 && !this->should_stop(i)) {
                ++i;

                // current top of the path
                auto* label = this->path_.back().first;

                // record solution if at sink
                if (label->get_end_node()->sink) {
                    if (label->get_cost() < this->cost_upper_bound_) {
                        if (label->get_cost() + this->params_.tolerance <
                            this->best_cost_upper_bound_) {
                            this->best_cost_upper_bound_ = label->get_cost();
                            LOG_INFO("Found a better solution with cost ", label->get_cost(), "\n");
                        }
                        this->extract_solution(*label);
                    }
                }

                // advance: dive deeper greedily, or backtrack to a sibling on dead-end
                advance();
            }

            LOG_DEBUG("GreedyAlgorithm: WHILE nb iter: ", i, "\n");
        }

        /// Greedy advance step: keep extending the deepest label until it cannot
        /// be extended further; if the very first attempt fails, backtrack and
        /// switch to the next sibling. After one call either @c path_ is empty,
        /// or the top has just been replaced (either by a deeper label or a
        /// sibling at the same depth).
        void advance() {
            while (!this->path_.empty()) {
                bool extended = false;
                while (this->extend_label(this->path_.back().first)) {
                    extended = true;
                }
                if (extended) {
                    return;
                }
                if (!this->backtrack()) {
                    return;
                }
            }
        }
};

}  // namespace rcspp
