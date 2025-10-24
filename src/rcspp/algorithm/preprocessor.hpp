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
            bool deleted = false;
            for (auto it = graph_->arcs_by_id_.begin(); it != graph_->arcs_by_id_.end();) {
                // check if we should remove the arc
                if (remove_arc(*it->second)) {
                    deleted_arcs_by_id_.push_back(it->first);
                    it = graph_->delete_arc(it);
                    deleted = true;
                } else {
                    ++it;
                }
            }
            return deleted;
        }

        virtual void restore() {
            for (const auto& arc_id : deleted_arcs_by_id_) {
                graph_->restore_arc(arc_id);
            }
            deleted_arcs_by_id_.clear();
        }

    private:
        Graph<ResourceType>* graph_;
        std::vector<size_t> deleted_arcs_by_id_;

    protected:
        bool disable_preprocessing_ = false;
        virtual bool remove_arc(const Arc<ResourceType>& arc) { return false; }
};
}  // namespace rcspp
