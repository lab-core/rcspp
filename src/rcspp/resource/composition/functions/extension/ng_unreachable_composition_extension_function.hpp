// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <map>
#include <vector>

#include "rcspp/general/clonable.hpp"
#include "rcspp/graph/arc.hpp"
#include "rcspp/resource/composition/functions/extension/composition_extension_function.hpp"
#include "rcspp/resource/composition/resource_type_composition.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief ng-route memory with Feillet-style unreachable-set augmentation.
///
/// Drop-in replacement for @ref CompositionExtensionFunction that, after the
/// standard per-component extension, folds the resource-unreachable members of
/// the destination node's ng-neighbourhood N_j into the ng-memory bitmask Π.
///
/// Reachability is tested the only general way — by actually building the trial
/// label along arc j->x and checking its feasibility (reachability is the
/// composition of extension and feasibility, so there is no node-only shortcut
/// that generalises across resources).  For every neighbour x in N_j that is not
/// already in Π, if the extension L->x is infeasible then x is unreachable and is
/// folded into Π.
///
/// This is **pure acceleration**: under monotone (non-decreasing) resources an
/// unreachable node is permanently unreachable, so forbidding it via Π never
/// removes a feasible route — the relaxation (LP bound) is unchanged, only the
/// number of non-dominated labels drops (dominance still uses plain inclusion on
/// the augmented Π).
///
/// @tparam NgResourceType The set/bitset resource type holding the ng-memory.
/// @tparam ResourceTypes The composition's component resource types.
template <typename NgResourceType, typename... ResourceTypes>
    requires ResourceTypeConcept<NgResourceType> && (ResourceTypeConcept<ResourceTypes> && ...)
class NgUnreachableCompositionExtensionFunction
    : public Clonable<NgUnreachableCompositionExtensionFunction<NgResourceType, ResourceTypes...>,
                      CompositionExtensionFunction<ResourceTypes...>,
                      ExtensionFunction<ResourceTypeComposition<ResourceTypes...>>> {
        using CompositionType = ResourceTypeComposition<ResourceTypes...>;
        using ArcType = Arc<CompositionType>;

    public:
        /// @brief Construct the augmenting extension function.
        ///
        /// @param ng_arcs_by_node_id For each node j, the outgoing arcs j->x to the
        ///        ng-neighbours x in N_j (must outlive this function and its clones;
        ///        may be populated after construction — it is read at extension time).
        /// @param ng_resource_index Occurrence index of the ng resource within its
        ///        type's component vector (0 when it is the only one of its type).
        explicit NgUnreachableCompositionExtensionFunction(
            const std::map<size_t, std::vector<const ArcType*>>* ng_arcs_by_node_id,
            size_t ng_resource_index = 0)
            : ng_arcs_by_node_id_(ng_arcs_by_node_id), ng_resource_index_(ng_resource_index) {}

    protected:
        const std::map<size_t, std::vector<const ArcType*>>* ng_arcs_by_node_id_;
        size_t ng_resource_index_;
        // Node the current arc extends into (set per arc by preprocess).
        size_t destination_id_ = 0;

        // Re-entrancy guard shared across all clones in a thread: the trial
        // extensions below reuse the (augmented) arc extenders, so without this
        // post_extend would recurse indefinitely.  Suppressing augmentation inside
        // a trial is also exactly what we want — the trial must measure the plain
        // extension's feasibility.
        static thread_local bool in_trial_extension_;

        void preprocess(size_t /*origin_id*/, size_t destination_id) override {
            destination_id_ = destination_id;
        }

        void post_extend(const Resource<CompositionType>& /*resource*/,
                         const Extender<CompositionType>& /*extender*/,
                         Resource<CompositionType>* extended_resource) override {
            if (in_trial_extension_ || ng_arcs_by_node_id_ == nullptr) {
                return;
            }
            auto it = ng_arcs_by_node_id_->find(destination_id_);
            if (it == ng_arcs_by_node_id_->end()) {
                return;
            }
            auto& ng = extended_resource->template get_component<NgResourceType>(ng_resource_index_)
                           .get_value();

            in_trial_extension_ = true;
            for (const ArcType* arc : it->second) {
                const size_t x = arc->destination->id;
                // Skip neighbours already remembered in the ng-memory.
                if (ng.contains(x)) {
                    continue;
                }
                // Build the trial label L->x and fold x in if it is infeasible.
                // The trial is built from x's prototype resource so that its
                // feasibility functions are preprocessed for x.
                Resource<CompositionType> trial(*arc->destination->resource);
                arc->extender->extend(*extended_resource, &trial);
                if (!trial.is_feasible()) {
                    ng.add(x);
                }
            }
            in_trial_extension_ = false;
        }
};

template <typename NgResourceType, typename... ResourceTypes>
    requires ResourceTypeConcept<NgResourceType> && (ResourceTypeConcept<ResourceTypes> && ...)
thread_local bool NgUnreachableCompositionExtensionFunction<
    NgResourceType, ResourceTypes...>::in_trial_extension_ = false;

}  // namespace rcspp
