// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/backward_form.hpp"
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
///
/// **Backward form: @c Mirror**, supplied by @c NodeMirrorForm. The formula is written once and
/// is direction-blind: it narrows against the neighborhood of the node the label is *leaving*,
/// then adds that node. Going forward that node is the arc's origin; going backward it is the
/// arc's destination. The form owns that swap, so this class never sees a direction -- which is
/// what makes the historical defect of using the wrong node inexpressible here.
///
/// **Behaviour note (changed in step 4).** The arc's extender value is ignored; the node added to
/// the memory is derived from the arc's own endpoints. Previously the forward direction used the
/// arc's value, which in every shipped model was the origin singleton. A model that deliberately
/// attaches other data to an ng arc must write its own `extend` against
/// `DeclaredKindForm<ExtensionFunction<R>, BackwardKind::Mirror>` instead.
///
/// @tparam ResourceType A ContainerResource-compatible type supporting `get_intersection()`,
///                      `get_union()`, `set_value()`, and `reset()`.
/// @tparam ValueType    Element type stored in the neighborhood sets.  Defaults to
///                      `ResourceType::ValueType`; can be overridden to select an
///                      alternative `set_value()` overload.
template <typename ResourceType, typename ValueType = typename ResourceType::ValueType>
class NgPathExtensionFunction
    : public Clonable<NgPathExtensionFunction<ResourceType, ValueType>,
                      NodeMirrorForm<ResourceType, ExtensionFunction<ResourceType>>,
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

    protected:
        // NodeMirrorForm is a DEPENDENT base -- it depends on ResourceType -- so unqualified
        // lookup does not find its nested `Side`. It has to be aliased, for the same reason
        // Resource<ResourceType> writes `this->value_` throughout. Without this alias the two
        // overrides below do not compile, with an error that does not obviously name the cause.
        using Side = typename NodeMirrorForm<ResourceType, ExtensionFunction<ResourceType>>::Side;

        /// @brief The ng-path formula, in whichever direction the form is asking about.
        ///
        /// @param resource          Current ng-path resource of the label.
        /// @param extended_resource Output: receives `(resource n neighborhood) u {node_left}`.
        /// @param side              The node the label is leaving, and its neighborhood.
        void apply(const ResourceType& resource, ResourceType* extended_resource,
                   const Side& side) const final {
            // Keep only the nodes in the neighborhood of the node being left, then add that node.
            auto narrowed = resource.get_intersection(side.neighborhood.get_value());
            narrowed = side.node_left.get_union(narrowed);
            extended_resource->set_value(narrowed);
        }

        /// @brief Loads one node's singleton and ng-neighborhood.
        ///
        /// @param node_left_id Index of the node the label leaves.
        /// @return That node's side. A node absent from the map gets an empty neighborhood,
        ///         i.e. no narrowing.
        [[nodiscard]] Side make_side(size_t node_left_id) const final {
            Side side;
            side.node_left.set_value(std::set<ValueType>{static_cast<ValueType>(node_left_id)});
            if (auto it = ng_neighborhood_by_origin_id_->find(node_left_id);
                it != ng_neighborhood_by_origin_id_->end()) {
                side.neighborhood.set_value(it->second);
            }  // else: default-constructed, i.e. empty -- absent means no narrowing
            return side;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> ng_neighborhood_by_origin_id_;
};

}  // namespace rcspp
