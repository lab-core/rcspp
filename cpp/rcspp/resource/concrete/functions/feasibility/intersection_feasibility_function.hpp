// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/functions/feasibility/merge_form.hpp"

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
/// A bidirectional solve refuses this function whenever it constrains anything (see
/// @ref merge_rule). Forward-only use is unrestricted.
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
    : public Clonable<
          IntersectionFeasibilityFunction<ContainerResourceType, ValueType>,
          DisjointMergeForm<ContainerResourceType, FeasibilityFunction<ContainerResourceType>>,
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
              forbidden_(forbidden) {
            // Whole-function, not per node, so every node resource caches the same merge rule.
            constrains_something_ = std::ranges::any_of(*values_by_node_id_, [](const auto& entry) {
                return !entry.second.empty();
            });
        }

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

        /// @brief @c Unspecified whenever this function constrains anything, so a bidirectional
        ///        solve refuses to start; @c AlwaysTrue when it constrains nothing.
        ///
        /// `is_feasible` is a predicate on a prefix, and the inherited `is_back_feasible` asks
        /// the same question of a suffix, which silently loses or admits wrong paths for both
        /// required and forbidden sets. An inert function must not narrow the join, hence
        /// @c AlwaysTrue when nothing is constrained.
        ///
        /// @return @c MergeRule::AlwaysTrue when nothing is constrained anywhere,
        ///         @c MergeRule::Unspecified otherwise.
        [[nodiscard]] MergeRule merge_rule() const override {
            return constrains_something_ ? MergeRule::Unspecified : MergeRule::AlwaysTrue;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> values_by_node_id_;
        ContainerResourceType values_;
        bool forbidden_;     // values are forbidden or required
        bool empty_ = true;  // to avoid checking intersection if no values to check

        /// @brief Whether any node's set is non-empty (whole-function, not per node).
        bool constrains_something_ = false;

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
