// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>    // NOLINT(build/c++11)
#include <concepts>  // NOLINT(build/include_order)
#include <iostream>
#include <limits>
#include <list>
#include <utility>
#include <vector>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_pool.hpp"

namespace rcspp {

template <typename ResourceType>
using LabelIteratorPair =
    std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>;

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class AlgorithmWithIterators : public Algorithm<ResourceType> {
    public:
        AlgorithmWithIterators(ResourceFactory<ResourceType>* resource_factory,
                               const Graph<ResourceType>& graph, bool use_pool = true)
            : Algorithm<ResourceType>(resource_factory, graph, use_pool) {}

        ~AlgorithmWithIterators() override = default;

        std::vector<Solution> solve(bool print = false) override {
            this->initialize_labels();

            int i = 0;

            while (this->number_of_labels() > 0) {
                i++;

                auto label_iterator_pair = next_label_iterator();

                auto& label = *label_iterator_pair.first;

                if (label.dominated) {
                    this->label_pool_.release_label(&label);
                    continue;
                }

                assert(label.get_end_node());

                if (this->graph_.is_sink(label.get_end_node()->id) &&
                    (label.get_cost() < this->cost_upper_bound_)) {
                    this->cost_upper_bound_ = label.get_cost();
                    this->best_label_ = &label;
                } else if (!this->graph_.is_sink(label.get_end_node()->id) &&
                           label.get_cost() < std::numeric_limits<double>::infinity()) {
                    bool label_non_dominated = update_non_dominated_labels(label_iterator_pair);

                    if (label_non_dominated) {
                        auto time_start = std::chrono::high_resolution_clock::now();
                        this->expand(&label);
                        auto time_end = std::chrono::high_resolution_clock::now();

                        this->total_full_expand_time_ +=
                            std::chrono::duration_cast<std::chrono::nanoseconds>(time_end -
                                                                                 time_start)
                                .count();
                    } else {
                        this->label_pool_.release_label(&label);
                    }
                } else {
                    this->label_pool_.release_label(&label);

                    remove_label(label_iterator_pair.second);
                }
            }

            std::cout << "****************************************\n";
            std::cout << "RCSPP: WHILE nb iter: " << i << std::endl;
            std::cout << "****************************************\n";

            std::cout << "best_label_=" << this->best_label_ << std::endl;

            std::vector<Solution> solutions;
            if (this->best_label_ != nullptr) {
                solutions = extract_solutions();

                std::cout << "Number of solutions: " << solutions.size() << std::endl;
            }

            return solutions;
        }

    protected:
        virtual LabelIteratorPair<ResourceType> next_label_iterator() = 0;

        virtual void remove_label(
            const std::list<Label<ResourceType>*>::iterator& label_iterator) = 0;

        virtual bool update_non_dominated_labels(
            const LabelIteratorPair<ResourceType>& label_iterator_pair) = 0;

        [[nodiscard]] virtual std::list<Label<ResourceType>*> get_labels_at_sinks() const = 0;

    private:
        std::vector<Solution> extract_solutions() {
            std::vector<Solution> solutions;

            auto labels_at_sinks = this->get_labels_at_sinks();
            for (const auto* sink_label : labels_at_sinks) {
                const auto path_node_ids = this->get_path_node_ids(*sink_label);
                const auto path_arc_ids = this->get_path_arc_ids(*sink_label);
                auto solution = Solution{sink_label->get_cost(), path_node_ids, path_arc_ids};

                solutions.push_back(solution);
            }

            std::ranges::sort(solutions,
                              [](const Solution& a, const Solution& b) { return a.cost < b.cost; });

            return solutions;
        }
};
}  // namespace rcspp
