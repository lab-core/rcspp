// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <vector>

#include "rcspp/graph/graph.hpp"

namespace rcspp {

/// @brief Base class for graph preprocessors that remove infeasible or dominated arcs.
///
/// A `Preprocessor` operates on a `Graph` and iteratively removes arcs that are deemed
/// unnecessary by the concrete subclass's `remove_arc` predicate.  Removed arcs can be
/// restored to their original state via `restore()`.
///
/// @tparam ResourceType The resource type used in the graph; must satisfy
///         `ResourceTypeConcept`.
template <typename ResourceType>
    requires ResourceTypeConcept<ResourceType>
class Preprocessor {
    public:
        /// @brief Constructs a preprocessor attached to the given graph.
        ///
        /// @param graph Non-owning pointer to the graph to preprocess.
        explicit Preprocessor(Graph<ResourceType>* graph) : graph_(graph) {}

        /// @brief Virtual destructor.
        virtual ~Preprocessor() = default;

        /// @brief Removes arcs that are identified as unnecessary by `remove_arc`.
        ///
        /// Iterates over all arcs and removes any arc for which `remove_arc` returns
        /// `true`.  If preprocessing is disabled (`disable_preprocessing_` is set), the
        /// method returns immediately without modifying the graph.
        ///
        /// @return `true` if at least one arc was removed, `false` otherwise.
        virtual bool preprocess() {
            if (disable_preprocessing_) {
                return false;
            }
            auto arc_ids = graph_->remove_arcs_if(
                [this](const Arc<ResourceType>& arc) { return remove_arc(arc); });
            removed_arcs_by_id_.insert(removed_arcs_by_id_.end(), arc_ids.begin(), arc_ids.end());
            return !arc_ids.empty();
        }

        /// @brief Restores all arcs that were removed by previous calls to `preprocess`.
        ///
        /// After this call the graph is in the same state it was before any preprocessing
        /// was applied.
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
        /// @brief When set to `true`, `preprocess()` becomes a no-op.
        ///
        /// Subclasses should set this flag when preprocessing cannot be safely performed
        /// (e.g., when a required bound is infinite or a negative cycle is detected).
        bool disable_preprocessing_ = false;

        /// @brief Determines whether a given arc should be removed from the graph.
        ///
        /// Subclasses override this method to implement their specific removal criterion.
        /// The default implementation never removes any arc.
        ///
        /// @param arc The arc to evaluate.
        /// @return `true` if the arc should be removed, `false` otherwise.
        virtual bool remove_arc(const Arc<ResourceType>& arc) { return false; }
};
}  // namespace rcspp
