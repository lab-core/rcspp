// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Feasibility function that checks whether the resource's container value intersects
///        a per-node set of values, treating the set as either forbidden or required.
///
/// At each graph node, a set of `ValueType` elements is optionally associated.
/// When @p forbidden is `true` (default), the label is infeasible if the resource
/// container value intersects that set (i.e., the set contains values that must be
/// avoided).  When @p forbidden is `false`, the label is feasible only if the
/// intersection is non-empty (i.e., the set contains values that must be present).
///
/// @tparam ContainerResourceType The resource type whose value is a container
///         supporting `intersects()` and `set_value()`.
/// @tparam ValueType The element type stored in the per-node sets; defaults to
///         `ContainerResourceType::ValueType`.
// ValueType is the element type stored in the per-node forbidden/required sets and is
// fed to ContainerResourceType::set_value. The default matches the element type the
// resource advertises; override it to point IFF at an alternative set_value overload.
template <typename ContainerResourceType,
          typename ValueType = typename ContainerResourceType::ValueType>
class IntersectionFeasibilityFunction
    : public Clonable<IntersectionFeasibilityFunction<ContainerResourceType, ValueType>,
                      FeasibilityFunction<ContainerResourceType>> {
    public:
        /// @brief Constructs the function with a per-node map of value sets.
        ///
        /// @param values_by_node_id Map from node id to the set of values to check at that node.
        /// @param forbidden If `true`, the label is infeasible when the resource intersects the
        ///        set (forbidden values); if `false`, the label is infeasible when there is no
        ///        intersection (required values).
        explicit IntersectionFeasibilityFunction(
            std::map<size_t, std::set<ValueType>> values_by_node_id, bool forbidden = true)
            : values_by_node_id_(std::make_shared<const std::map<size_t, std::set<ValueType>>>(
                  std::move(values_by_node_id))),
              forbidden_(forbidden) {}

        /// @brief Checks whether the resource satisfies the intersection constraint at the
        ///        current node.
        ///
        /// Returns `true` immediately when no values are associated with the current node.
        /// Otherwise returns `true` iff the intersection condition matches the configured
        /// `forbidden` semantics.
        ///
        /// @param resource The container resource to evaluate.
        /// @return `true` if the label is feasible; `false` otherwise.
        auto is_feasible(const ContainerResourceType& resource) -> bool override {
            if (empty_) {
                return true;  // no values to check, always feasible
            }
            // if forbidden, return true if no intersection
            // if required (forbidden_ = false), return true if intersection
            return resource.intersects(values_.get_value()) ^ forbidden_;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> values_by_node_id_;
        ContainerResourceType values_;
        bool forbidden_;     // values are forbidden or required
        bool empty_ = true;  // to avoid checking intersection if no values to check

        void preprocess(size_t node_id) override {
            if (values_by_node_id_ == nullptr) {
                return;
            }
            auto it = values_by_node_id_->find(node_id);
            if (it != values_by_node_id_->end()) {
                values_.set_value(it->second);
                empty_ = it->second.empty();
            } else {
                empty_ = true;
            }
        }
};
}  // namespace rcspp
