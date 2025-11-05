// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cassert>
#include <chrono>    // NOLINT(build/c++11)
#include <concepts>  // NOLINT(build/include_order)
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "rcspp/algorithm/solution.hpp"
#include "rcspp/graph/graph.hpp"
#include "rcspp/label/label_pool.hpp"
#include "rcspp/utils/timer.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Algorithm {
    public:
        Algorithm(ResourceFactory<ResourceType>* resource_factory, const Graph<ResourceType>& graph,
                  bool use_pool = true)
            : label_pool_(LabelPool<ResourceType>(
                  std::make_unique<LabelFactory<ResourceType>>(resource_factory), use_pool)),
              graph_(graph),
              cost_upper_bound_(std::numeric_limits<double>::infinity()),
              best_label_(nullptr) {}

        virtual ~Algorithm() = default;

        virtual std::vector<Solution> solve(bool print = false) {
            print_ = print;

            initialize_labels();

            int nb_iter = 0;

            while (number_of_labels() > 0) {
                nb_iter++;

                auto& label = next_label();

                if (label.dominated) {
                    this->label_pool_.release_label(&label);
                    continue;
                }

                assert(label.get_end_node());

                if (graph_.is_sink(label.get_end_node()->id) &&
                    (label.get_cost() < cost_upper_bound_)) {
                    cost_upper_bound_ = label.get_cost();
                    best_label_ = &label;
                } else if (!graph_.is_sink(label.get_end_node()->id) &&
                           label.get_cost() < std::numeric_limits<double>::infinity()) {
                    total_full_expand_time_.start();
                    expand(&label);
                    total_full_expand_time_.stop();
                } else {
                    this->label_pool_.release_label(&label);

                    remove_label(label);
                }
            }

            LOG_DEBUG("RCSPP: WHILE nb iter: ", nb_iter, "\n");
            LOG_TRACE("best_label_=", best_label_, "\n");

            Solution solution;
            if (best_label_ != nullptr) {
                const auto& best_label_res = best_label_->get_resource();

                auto cost = best_label_res.template get_resource_component<0>(0).get_value();
                auto time = best_label_res.template get_resource_component<0>(1).get_value();
                auto demand = best_label_res.template get_resource_component<0>(2).get_value();

                const auto path_node_ids = get_path_node_ids(*best_label_);
                const auto path_arc_ids = get_path_arc_ids(*best_label_);

                for (auto node_id : path_node_ids) {
                    LOG_DEBUG(node_id, ", ");
                }
                LOG_DEBUG('\n');

                for (auto arc_id : path_arc_ids) {
                    LOG_DEBUG(arc_id, ", ");
                }
                LOG_DEBUG('\n');

                solution = Solution{best_label_->get_cost(), path_node_ids, path_arc_ids};
            }

            return std::vector<Solution>{solution};
        }

    protected:
        bool print_{false};

        virtual void initialize_labels() = 0;

        virtual Label<ResourceType>& next_label() = 0;

        virtual bool test(const Label<ResourceType>& label) = 0;

        virtual void expand(Label<ResourceType>* label) = 0;

        [[nodiscard]] virtual size_t number_of_labels() const = 0;

        virtual std::vector<size_t> get_path_node_ids(const Label<ResourceType>& label) = 0;

        virtual std::vector<size_t> get_path_arc_ids(const Label<ResourceType>& label) = 0;

        virtual std::vector<std::pair<std::vector<size_t>, std::vector<double>>>
        get_vector_paths_node_ids(const Label<ResourceType>& label) = 0;

        virtual void remove_label(const Label<ResourceType>& label) = 0;

        LabelPool<ResourceType> label_pool_;

        const Graph<ResourceType>& graph_;

        double cost_upper_bound_;

        Label<ResourceType>* best_label_;

        size_t nb_dominated_labels_{0};

        Timer total_full_expand_time_;
};
}  // namespace rcspp
