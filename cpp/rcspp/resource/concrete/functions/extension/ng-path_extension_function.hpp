// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/arc_endpoints_extension_form.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

namespace ng_path_detail {

/// @brief Whether this process has not yet warned about an ng arc that carries a value.
///
/// @return @c true the first time it is asked.
inline bool first_report_of_arc_value() {
    static std::atomic<bool> reported{false};
    return !reported.exchange(true);
}

}  // namespace ng_path_detail

/// @brief Extension function implementing the ng-path relaxation for set-based resources.
///
/// In the ng-path relaxation a label keeps track of the set of nodes that may form
/// a cycle with the current partial path.  When traversing an arc from `origin` to
/// `destination`, the new set is computed as:
///
///   `(current_set ∪ {origin}) ∩ ng_neighborhood[destination]`
///
/// i.e. join the node being left, then narrow by the neighborhood of the node arrived at. The
/// stored set is thus the memory on leaving the node, which lets forward and backward halves
/// meeting at a node compare their memories directly.
///
/// Directional resource kind: @c ArcEndpoints, via @c ArcEndpointsExtensionForm, which swaps origin
/// and destination per direction. The arc's extender value is ignored; the node added to the memory
/// comes from the arc's endpoints.
///
/// @c extend and @c extend_back come from @c ArcEndpointsExtensionForm: both call @ref apply, with
/// the arc's ends in forward or reverse order. This class only gives the formula and its sets.
///
/// @tparam ResourceType A ContainerResource-compatible type supporting `get_intersection()`,
///                      `get_union()`, `set_value()`, and `reset()`.
/// @tparam ValueType    Element type stored in the neighborhood sets.  Defaults to
///                      `ResourceType::ValueType`; can be overridden to select an
///                      alternative `set_value()` overload.
template <typename ResourceType, typename ValueType = typename ResourceType::ValueType>
class NgPathExtensionFunction
    : public Clonable<NgPathExtensionFunction<ResourceType, ValueType>,
                      ArcEndpointsExtensionForm<ResourceType, ExtensionFunction<ResourceType>>,
                      ExtensionFunction<ResourceType>> {
    public:
        /// @brief Constructs an NgPathExtensionFunction with the per-node neighborhoods.
        ///
        /// @param ng_neighborhood_by_node_id Map from node id to its ng-neighborhood set. A node
        ///                                   absent from the map narrows to `{itself}`, forgetting
        ///                                   everything.
        explicit NgPathExtensionFunction(
            std::map<size_t, std::set<ValueType>> ng_neighborhood_by_node_id)
            : ng_neighborhood_by_node_id_(
                  std::make_shared<const std::map<size_t, std::set<ValueType>>>(
                      std::move(ng_neighborhood_by_node_id))),
              node_sets_(std::make_shared<NodeSets>()) {}

    protected:
        // `Side` lives in a dependent base, so it must be named explicitly.
        using Side =
            typename ArcEndpointsExtensionForm<ResourceType, ExtensionFunction<ResourceType>>::Side;

        /// @brief The ng-path formula, shared by both directions. Called by
        ///        @c ArcEndpointsExtensionForm::extend and @c extend_back.
        ///
        /// Adds the node just left, then keeps only the members of the arrival node's neighborhood.
        ///
        /// @param resource          Current ng-path resource of the label.
        /// @param extended_resource Output: receives
        ///                          `(resource ∪ {node_left}) ∩ arrival_neighborhood`.
        /// @param side              The node the label leaves, and the neighborhood of the node it
        ///                          arrives at.
        void apply(const ResourceType& resource, ResourceType* extended_resource,
                   const Side& side) const final {
            // In place, reusing the label's buffer: this is the solver's hottest path.
            extended_resource->assign_union(resource.get_value(), side.node_left->get_value());
            extended_resource->intersect_with(side.arrival_neighborhood->get_value());
        }

        /// @brief In a debug build, warns once if an arc carries a value other than empty or
        ///        `{origin}`, since that value is ignored.
        ///
        /// An empty value is ignored too, but not reported: callers pass one as a placeholder.
        ///
        /// @param extender_value The arc's value.
        /// @param side           This traversal's node left and arrival neighborhood.
        void check_arc_value(const ResourceType& extender_value, const Side& side) const final {
            const bool placeholder_or_origin =
                extender_value.empty() || (extender_value.size() == 1 &&
                                           extender_value.includes(side.node_left->get_value()));
            if (!placeholder_or_origin && ng_path_detail::first_report_of_arc_value()) {
                LOG_WARN("NgPathExtensionFunction: an arc carries ",
                         extender_value.to_string(),
                         ", which is ignored: the node added to the memory is read from the arc's "
                         "endpoints. (Reported once per process, in a debug build.)\n");
            }
        }

        /// @brief Loads the singleton of the node left and the neighborhood of the node arrived at.
        ///
        /// The arrival node is always added to its own neighborhood, so the feasibility test
        /// "is `v` in my memory at `v`" can fire. A node absent from the map narrows to `{itself}`.
        ///
        /// Each set is built once per node, the first time an arc reads it, and shared by every
        /// clone of this function: one copy per node rather than one per arc.
        ///
        /// @param node_left_id    Index of the node the label leaves.
        /// @param node_arrived_id Index of the node the label arrives at.
        /// @return That traversal's side.
        [[nodiscard]] Side make_side(size_t node_left_id, size_t node_arrived_id) const final {
            const std::scoped_lock lock(node_sets_->mutex);
            auto& node_left = node_sets_->singletons[node_left_id];
            if (node_left == nullptr) {
                node_left = std::make_unique<ResourceType>();
                node_left->set_value(std::set<ValueType>{static_cast<ValueType>(node_left_id)});
            }
            auto& arrival = node_sets_->arrival_neighborhoods[node_arrived_id];
            if (arrival == nullptr) {
                std::set<ValueType> members;
                if (auto it = ng_neighborhood_by_node_id_->find(node_arrived_id);
                    it != ng_neighborhood_by_node_id_->end()) {
                    members = it->second;
                }
                members.insert(static_cast<ValueType>(node_arrived_id));
                arrival = std::make_unique<ResourceType>();
                arrival->set_value(members);
            }
            return {.node_left = node_left.get(), .arrival_neighborhood = arrival.get()};
        }

    private:
        /// @brief The per-node sets the sides point at, built on first use. Never shrinks, so
        ///        a pointer handed out stays valid while any clone lives.
        struct NodeSets {
                std::mutex mutex;
                std::unordered_map<size_t, std::unique_ptr<ResourceType>> singletons;
                std::unordered_map<size_t, std::unique_ptr<ResourceType>> arrival_neighborhoods;
        };

        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> ng_neighborhood_by_node_id_;
        std::shared_ptr<NodeSets> node_sets_;
};

}  // namespace rcspp
