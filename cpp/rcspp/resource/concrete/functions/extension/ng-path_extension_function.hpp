// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

/// @brief Extension function implementing the ng-path relaxation for set-based resources.
///
/// In the ng-path relaxation a label keeps track of the set of nodes that may form
/// a cycle with the current partial path.  When traversing an arc from `origin` to
/// `destination`, the new reachable set is computed as:
///
///   `(current_set ∩ ng_neighborhood[origin]) ∪ {origin}`
///
/// where `ng_neighborhood[origin]` is the pre-defined neighborhood of the origin node.
/// The forward extension uses the origin's neighborhood; the backward extension uses the
/// destination's neighborhood.  Both neighborhoods are looked up once per arc in
/// `preprocess()` and cached for efficiency.
///
/// @tparam ResourceType A ContainerResource-compatible type supporting `get_intersection()`,
///                      `get_union()`, `set_value()`, and `reset()`.
/// @tparam ValueType    Element type stored in the neighborhood sets.  Defaults to
///                      `ResourceType::ValueType`; can be overridden to select an
///                      alternative `set_value()` overload.
template <typename ResourceType, typename ValueType = typename ResourceType::ValueType>
class NgPathExtensionFunction : public Clonable<NgPathExtensionFunction<ResourceType, ValueType>,
                                                ExtensionFunction<ResourceType>> {
    public:
        /// @brief Constructs an NgPathExtensionFunction with the per-node neighborhoods.
        ///
        /// @param ng_neighborhood_by_origin_id  Map from node id to its ng-neighborhood set.
        ///                                      Nodes absent from the map are treated as having
        ///                                      an empty neighborhood.
        explicit NgPathExtensionFunction(
            std::map<size_t, std::set<ValueType>> ng_neighborhood_by_origin_id)
            : ng_neighborhood_by_origin_id_(
                  std::make_shared<const std::map<size_t, std::set<ValueType>>>(
                      std::move(ng_neighborhood_by_origin_id))) {}

        /// @brief Forward extension: intersects @p resource with the origin's neighborhood,
        /// then unions with @p extender_value.
        ///
        /// @param resource           Current ng-path resource of the label.
        /// @param extender_value     Arc's extender resource (typically represents the origin
        /// node).
        /// @param extended_resource  Output: receives the new ng-path set.
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            extend(resource, extender_value, extended_resource, ng_neighborhood_);
        }

        /// @brief Backward extension: intersects @p resource with the destination's
        /// neighborhood, then unions with @p extender_value.
        ///
        /// @param resource           Current ng-path resource of the backward label.
        /// @param extender_value     Arc's extender resource (typically represents the
        /// destination node).
        /// @param extended_resource  Output: receives the new ng-path set.
        void extend_back(const ResourceType& resource, const ResourceType& extender_value,
                         ResourceType* extended_resource) override {
            extend(resource, extender_value, extended_resource, ng_neighborhood_back_);
        }

        /// @brief Core ng-path extension with an explicit @p ng_neighborhood resource.
        ///
        /// Computes `(resource ∩ ng_neighborhood) ∪ extender_value` and stores the
        /// result in @p extended_resource.
        ///
        /// @param resource           Current ng-path resource.
        /// @param extender_value     Arc's extender resource (origin or destination node set).
        /// @param extended_resource  Output: receives the computed ng-path set.
        /// @param ng_neighborhood    Neighborhood to intersect with (pre-loaded by preprocess).
        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource, const ResourceType& ng_neighborhood) {
            // keep only the nodes in the neighborhood of the origin node of the arc
            auto intersection_container = resource.get_intersection(ng_neighborhood.get_value());
            // then, add the extender value (which is the origin node of the arc normally)
            intersection_container = extender_value.get_union(intersection_container);
            extended_resource->set_value(intersection_container);
        }

    private:
        // neighborhood of the origin node of the arc
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> ng_neighborhood_by_origin_id_;
        ResourceType ng_neighborhood_;
        ResourceType ng_neighborhood_back_;

        void preprocess(size_t origin_id, size_t destination_id) override {
            // If the id is in the map, load its neighborhood; otherwise reset to empty so
            // we do not inherit the previous arc's binding.
            if (auto it = ng_neighborhood_by_origin_id_->find(origin_id);
                it != ng_neighborhood_by_origin_id_->end()) {
                ng_neighborhood_.set_value(it->second);
            } else {
                ng_neighborhood_.reset();
            }

            if (auto it = ng_neighborhood_by_origin_id_->find(destination_id);
                it != ng_neighborhood_by_origin_id_->end()) {
                ng_neighborhood_back_.set_value(it->second);
            } else {
                ng_neighborhood_back_.reset();
            }
        }
};

}  // namespace rcspp
