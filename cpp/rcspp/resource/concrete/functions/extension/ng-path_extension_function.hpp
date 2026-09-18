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
/// `destination`, the new set is computed as:
///
///   `(current_set ∪ {origin}) ∩ ng_neighborhood[destination]`
///
/// — join the node being left, then narrow by the neighborhood of the node being *arrived at*.
///
/// **The stored set is the memory as it will be on LEAVING this node**, not as it was on
/// arriving. The two differ by one narrowing, and that narrowing is what the bidirectional join
/// needs: a forward half and a backward half meeting at `v` compare their memories there, and the
/// forward half's has to have been filtered by `ng(v)` already or the comparison is one step
/// stale. Storing the pre-narrowing set made `DisjointMergeForm` reject concatenations the
/// forward search accepts. See @c merge_form.hpp.
///
/// This changes what the label *stores*, not which paths are feasible. Feasibility on extending
/// `a -> b` is `b ∉ memory_on_leaving(a)` either way: with the pre-narrowing set that reads
/// `b ∉ (Π(a) ∩ ng(a)) ∪ {a}`, and with this one it reads `b ∉ (Σ(a) ∪ {a}) ∩ ng(b)`, which is
/// the same test because `b ∈ ng(b)` -- see @c make_side. The stored set is also *smaller*, and
/// it is a sufficient statistic where the other was not, so @c InclusionDominanceFunction
/// dominates strictly more and the label sets shrink.
///
/// **Backward form: @c Mirror**, supplied by @c NodeMirrorForm. The formula is written once and
/// is direction-blind. Going forward the node left is the arc's origin and the node arrived at is
/// its destination; going backward, the other way round. The form owns that swap, so this class
/// never sees a direction -- which is what makes the historical defect of using the wrong node
/// inexpressible here.
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
        /// @param ng_neighborhood_by_node_id Map from node id to its ng-neighborhood set. A node
        ///                                   absent from the map narrows to `{itself}`, i.e. it
        ///                                   forgets everything -- see @c make_side.
        explicit NgPathExtensionFunction(
            std::map<size_t, std::set<ValueType>> ng_neighborhood_by_node_id)
            : ng_neighborhood_by_node_id_(
                  std::make_shared<const std::map<size_t, std::set<ValueType>>>(
                      std::move(ng_neighborhood_by_node_id))) {}

    protected:
        // NodeMirrorForm is a DEPENDENT base -- it depends on ResourceType -- so unqualified
        // lookup does not find its nested `Side`. It has to be aliased, for the same reason
        // Resource<ResourceType> writes `this->value_` throughout. Without this alias the two
        // overrides below do not compile, with an error that does not obviously name the cause.
        using Side = typename NodeMirrorForm<ResourceType, ExtensionFunction<ResourceType>>::Side;

        /// @brief The ng-path formula, in whichever direction the form is asking about.
        ///
        /// **Union first, then intersect.** The other order leaves the node just left unfiltered
        /// by the node just arrived at, which is precisely the one-step-stale memory the join
        /// cannot compare. It also matters for the node itself: a label arriving at `b` while
        /// still remembering `b` must keep `b` in the result, because that is what
        /// @c IntersectionFeasibilityFunction tests at `b`.
        ///
        /// @param resource          Current ng-path resource of the label.
        /// @param extended_resource Output: receives
        ///                          `(resource u {node_left}) n arrival_neighborhood`.
        /// @param side              The node the label leaves, and the neighborhood of the node it
        ///                          arrives at.
        void apply(const ResourceType& resource, ResourceType* extended_resource,
                   const Side& side) const final {
            // In place, into the destination's own storage. This runs once per label extension --
            // the hottest path in the solver -- and the obvious spelling,
            //
            //   extended->set_value(side.node_left.get_union(resource.get_value()));
            //   extended->set_value(extended->get_intersection(side.arrival.get_value()));
            //
            // allocates a container per call and frees the one it replaces, twice, because
            // `get_union` and `get_intersection` both return by value. `assign_union` and
            // `intersect_with` produce bit-identical results -- same word count, same contents --
            // and reuse the buffer a recycled label already holds. See
            // `ContainerResource::assign_union`.
            extended_resource->assign_union(resource.get_value(), side.node_left.get_value());
            extended_resource->intersect_with(side.arrival_neighborhood.get_value());
        }

        /// @brief Loads the singleton of the node left and the neighborhood of the node arrived at.
        ///
        /// **The arrival node is unioned into its own neighborhood**, and that is load-bearing
        /// rather than tidying: the feasibility test at a node asks whether the node is in the
        /// memory it arrives with, and a node filtered out of its own neighborhood could never be.
        /// It is otherwise inert. Whether `p ∈ ng(p)` is consulted only when a walk returns to `p`,
        /// and there are only two cases: `p` was still remembered, in which case the walk is
        /// ng-infeasible and this is exactly the rejection wanted; or `p` had been forgotten, in
        /// which case it is absent from the memory and the membership question does not arise. No
        /// surviving label's contents change.
        ///
        /// A node absent from the map therefore narrows to `{itself}`: it forgets everything,
        /// which is what an empty neighborhood has always meant here, while still supporting its
        /// own feasibility test.
        ///
        /// @param node_left_id    Index of the node the label leaves.
        /// @param node_arrived_id Index of the node the label arrives at.
        /// @return That traversal's side.
        [[nodiscard]] Side make_side(size_t node_left_id, size_t node_arrived_id) const final {
            Side side;
            side.node_left.set_value(std::set<ValueType>{static_cast<ValueType>(node_left_id)});

            std::set<ValueType> arrival;
            if (auto it = ng_neighborhood_by_node_id_->find(node_arrived_id);
                it != ng_neighborhood_by_node_id_->end()) {
                arrival = it->second;
            }
            arrival.insert(static_cast<ValueType>(node_arrived_id));
            side.arrival_neighborhood.set_value(arrival);
            return side;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> ng_neighborhood_by_node_id_;
};

}  // namespace rcspp
