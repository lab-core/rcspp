// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"
#include "rcspp/resource/functions/feasibility/merge_form.hpp"

namespace rcspp {

/// @brief Whether an element of the memory can be compared against a node id.
///
/// Used by @c IntersectionFeasibilityFunction to detect the ng-route condition
/// (`forbidden(v) = {v}`); element types failing this are treated as not holding node ids.
template <typename ValueType>
concept ComparableToNodeId = requires(const ValueType& element, size_t node_id) {
    { element == static_cast<ValueType>(node_id) } -> std::convertible_to<bool>;
};

/// @brief Feasibility function that checks whether the resource's container value intersects
///        a per-node set of values, treating the set as either forbidden or required.
///
/// At each graph node, a set of `ValueType` elements is optionally associated.
/// When @p forbidden is `true` (default), the label is infeasible if the resource
/// container value intersects that set (i.e., the set contains values that must be
/// avoided).  When @p forbidden is `false`, the label is feasible only if the
/// intersection is non-empty (i.e., the set contains values that must be present).
///
/// Bidirectional use requires the ng-route condition, `forbidden(v) = {v}` at every node that has
/// an entry (see @c presets::add_ng_path_resource); other entries are refused at setup. A node with
/// no entry may be revisited, and the merge test lets two halves share it. Forward-only use is
/// unrestricted.
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
            // Whole-function, not per node: the disjointness test checks overlap on any node.
            constrains_something_ = std::ranges::any_of(*values_by_node_id_, [](const auto& entry) {
                return !entry.second.empty();
            });

            // Whether every constraining entry is the node's own singleton (the ng-route
            // condition). Conservatively false for element types not comparable to node ids.
            if constexpr (ComparableToNodeId<ValueType>) {
                self_forbidden_only_ =
                    std::ranges::all_of(*values_by_node_id_, [](const auto& entry) {
                        return entry.second.empty() ||
                               (entry.second.size() == 1 &&
                                *entry.second.begin() == static_cast<ValueType>(entry.first));
                    });
                std::set<ValueType> self_forbidden;
                for (const auto& [node_id, values] : *values_by_node_id_) {
                    if (!values.empty()) {
                        self_forbidden.insert(static_cast<ValueType>(node_id));
                    }
                }
                self_forbidden_nodes_.set_value(self_forbidden);
            } else {
                self_forbidden_only_ = false;
            }
        }

        /// @brief Two halves conflict only on a node both remember **and** that is forbidden at
        ///        itself.
        ///
        /// Plain disjointness would also refuse a node both halves remember but that has no
        /// forbidden entry, which the model lets a path revisit. Exact under the ng-route
        /// condition, the only configuration whose merge rule is @c Custom.
        ///
        /// @param resource      The forward label's value.
        /// @param back_resource The backward label's value.
        /// @return @c true unless the two share a self-forbidden node.
        [[nodiscard]] auto can_be_merged(const ContainerResourceType& resource,
                                         const ContainerResourceType& back_resource)
            -> bool override {
            if (!resource.intersects(back_resource.get_value())) {
                return true;
            }
            ContainerResourceType shared(resource);
            shared.intersect_with(back_resource.get_value());
            return !shared.intersects(self_forbidden_nodes_.get_value());
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

        /// @brief Merge rule, depending on whether and what the function constrains.
        ///
        /// - Nothing constrained anywhere: @c AlwaysTrue, so an inert component never narrows the
        ///   join.
        /// - Every node forbids exactly itself (ng-route condition): @c Custom, using the
        ///   disjointness test from @c DisjointMergeForm.
        /// - Anything else (required values, or forbidden sets other than `{v}`): @c Unspecified,
        ///   so a bidirectional solve refuses to start. `is_feasible` is a predicate on a prefix,
        ///   and only the self-forbidden form reads correctly on a backward suffix.
        ///
        /// @return The merge rule for this configuration.
        [[nodiscard]] MergeRule merge_rule() const override {
            if (!constrains_something_) {
                return MergeRule::AlwaysTrue;
            }
            if (!forbidden_) {
                return MergeRule::Unspecified;
            }
            return self_forbidden_only_ ? MergeRule::Custom : MergeRule::Unspecified;
        }

        /// @brief Whether this function carries the ng-route condition, whose backward reading
        ///        needs a memory at `v` that excludes `v` (@c BackwardKind::EndpointMirror).
        ///
        /// @return @c true exactly when @ref merge_rule answers @c Custom.
        [[nodiscard]] bool requires_endpoint_mirror() const override {
            return forbidden_ && constrains_something_ && self_forbidden_only_;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> values_by_node_id_;
        ContainerResourceType values_;
        /// @brief The nodes whose entry forbids them at themselves; see @ref can_be_merged.
        ContainerResourceType self_forbidden_nodes_;
        bool forbidden_;     // values are forbidden or required
        bool empty_ = true;  // to avoid checking intersection if no values to check

        /// @brief Whether any node forbids anything. Whole-function, not per node; see
        ///        @ref merge_rule.
        bool constrains_something_ = false;

        /// @brief Whether every constraining entry forbids exactly its own node (the ng-route
        ///        condition). Whole-function, not per node; see @ref merge_rule.
        bool self_forbidden_only_ = false;

        void preprocess(size_t node_id) override {
            // values_by_node_id_ is never null: the constructor always allocates it.
            auto it = values_by_node_id_->find(node_id);
            if (it != values_by_node_id_->end()) {
                values_.set_value(it->second);
                empty_ = it->second.empty();
            } else {
                empty_ = true;
            }
        }
};

/// A container feasibility function has no scalar bound, so it never seeds a backward label.
template <typename R, typename V>
struct BackSeedEndOf<IntersectionFeasibilityFunction<R, V>> {
        static constexpr BackSeedEnd value = BackSeedEnd::Never;
};

}  // namespace rcspp
