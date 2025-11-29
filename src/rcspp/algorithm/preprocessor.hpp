// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/graph/graph.hpp"

namespace rcspp {

template <typename ResourceType>
    requires std::derived_from<ResourceType, ResourceBase<ResourceType>>
class Preprocessor {
    public:
        explicit Preprocessor(Graph<ResourceType>* graph) : graph_(graph) {}
        virtual ~Preprocessor() = default;

        virtual bool preprocess() {
            if (disable_preprocessing_) {
                return false;
            }
            auto arc_ids = graph_->remove_arcs_if(
                [this](const Arc<ResourceType>& arc) { return remove_arc(arc); });
            removed_arcs_by_id_.insert(removed_arcs_by_id_.end(), arc_ids.begin(), arc_ids.end());
            return !arc_ids.empty();
        }

        virtual void restore() {
            for (const auto& arc_id : removed_arcs_by_id_) {
                graph_->restore_arc(arc_id);
            }
            removed_arcs_by_id_.clear();
        }

    private:
        Graph<ResourceType>* graph_;
        std::vector<size_t> removed_arcs_by_id_;

    protected:
        bool disable_preprocessing_ = false;
        virtual bool remove_arc(const Arc<ResourceType>& arc) { return false; }
};
}  // namespace rcspp
