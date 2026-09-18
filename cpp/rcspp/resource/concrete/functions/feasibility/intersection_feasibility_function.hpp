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
/// Read by @c IntersectionFeasibilityFunction to decide whether its forbidden sets express the
/// *ng-route condition* -- "node `v` may not be revisited" -- which is the precondition
/// @c DisjointMergeForm needs. An element type that cannot even be compared to a node id is not
/// holding node identities, so that question has no answer for it and the join is refused; see
/// @c IntersectionFeasibilityFunction::merge_rule.
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
/// **Bidirectional use needs the forbidden sets to be the ng-route condition**, i.e.
/// `forbidden(v) = {v}` at every node that constrains anything. That is what
/// @c presets::add_ng_path_resource is for, and it is what every model in this repository
/// passes. Any other forbidden set is refused at setup rather than solved, because this class's
/// `is_feasible` is a predicate on a *prefix* and only the self-forbidden form of it has a
/// backward reading; @ref merge_rule says which and why. Forward-only use is unrestricted.
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
            // Whether this function forbids anything ANYWHERE, computed once. Not per node: the
            // disjointness merge test asks whether the two halves share any node at all, so a
            // per-node answer at the merge node would miss an overlap on a node that is forbidden
            // somewhere else. See merge_rule().
            constrains_something_ = std::ranges::any_of(*values_by_node_id_, [](const auto& entry) {
                return !entry.second.empty();
            });

            // Whether every constraining entry is the node's OWN singleton, i.e. whether these
            // forbidden sets are the ng-route condition. Whole-function and computed once, for
            // the same reason constrains_something_ is. See merge_rule().
            //
            // An element type that cannot be compared to a node id is not holding node
            // identities, so the question has no answer and the conservative one is "no".
            if constexpr (ComparableToNodeId<ValueType>) {
                self_forbidden_only_ =
                    std::ranges::all_of(*values_by_node_id_, [](const auto& entry) {
                        return entry.second.empty() ||
                               (entry.second.size() == 1 &&
                                *entry.second.begin() == static_cast<ValueType>(entry.first));
                    });
            } else {
                self_forbidden_only_ = false;
            }
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

        /// @brief The merge test depends on which way the constraint points, and -- when it
        ///        forbids -- on *what* it forbids.
        ///
        /// **Forbidden values, each node forbidding itself** (`forbidden(v) = {v}`, the ng-route
        /// condition): the two halves must not both contain a forbidden node, or the merged path
        /// visits it twice -- so the disjointness body inherited from @c DisjointMergeForm
        /// applies, and the rule is @c Custom.
        ///
        /// **Forbidden values that are anything else**: @c Unspecified, so a bidirectional solve
        /// refuses to start. This is the same prefix/suffix fault as the required case below, and
        /// it is worth spelling out because the class name suggests the forbidden direction is
        /// uniformly safe. It is not:
        ///
        ///  - `is_feasible` is `resource.intersects(values_)` -- *what has this label collected so
        ///    far*, a predicate on a **prefix**. `is_back_feasible` is inherited unchanged, so a
        ///    backward label's **suffix** is asked it too. With `forbidden(v) = {v}` the two
        ///    readings coincide: forward it means "the prefix already visited `v`", backward "the
        ///    suffix visits `v` again", and the cross case is exactly what disjointness tests.
        ///    That coincidence is what makes the self-forbidden form -- and only it -- have a
        ///    backward reading at all.
        ///  - With `forbidden(u) = {w}` for some `w != u` it breaks in the direction that matters
        ///    most: a forward half can collect `w` before the meeting node while the suffix passes
        ///    through `u`, and **nothing checks it**. The backward label never sees `w`, so it is
        ///    admitted; disjointness never sees `w` either, because `w` is not on the suffix. The
        ///    solve returns a path the model forbids and reports COMPLETE. Measured on a five-node
        ///    graph with `forbidden(3) = {1}`: `simple` returned -2 and `bidirectional` -13 on the
        ///    path `0 1 2 3 4`, which is infeasible at node 3.
        ///  - A forbidden set that binds nothing (`forbidden(2) = {9}` where 9 is never collected)
        ///    fails the other way: the model permits every revisit, but disjointness still gates
        ///    the join, so `bidirectional` answers a stricter question than `simple`. Measured:
        ///    -24 against -13, COMPLETE. That is review finding D6 surviving its own fix, because
        ///    @ref constrains_something_ -- "is any set non-empty" -- is a proxy for the real
        ///    precondition rather than the precondition.
        ///
        /// Both are closed by the same test, @ref self_forbidden_only_, which asks the
        /// precondition directly. Relaxing the rule to @c AlwaysTrue instead would close neither:
        /// the first fault is not in the merge, it is in `is_back_feasible`, and @c AlwaysTrue
        /// only stops the joiner asking. Refusing at setup is the honest answer, as it is for the
        /// required case.
        ///
        /// **Required** values: @c Unspecified, so a bidirectional solve refuses to start.
        ///
        /// This used to answer @c AlwaysTrue on the reasoning that "did the path collect
        /// everything" is a property of the *whole* path, already enforced at the endpoints, and
        /// that two halves cannot violate it by being combined. The second half of that is true.
        /// The first is not, and the merge rule is the wrong place to look:
        ///
        /// `is_feasible` here is `resource.intersects(values_)`, i.e. **the set collected SO FAR
        /// must already contain one of this node's required values**. That is a predicate on a
        /// *prefix*. A backward label's set is a *suffix*, and `is_back_feasible` is inherited
        /// from @c FeasibilityFunction, so it applies the prefix predicate to it. A required value
        /// that the forward half collects before the meeting node therefore rejects the backward
        /// half outright -- the pair never reaches the join, and the merge rule, whatever it says,
        /// is never consulted about it. The path is lost and the solve reports COMPLETE.
        ///
        /// Relaxing @c is_back_feasible to @c true instead would be worse, not better: the
        /// backward half would become legal and nothing would then check the requirement on the
        /// joined path at all, because @c AlwaysTrue means the joiner never asks. That trades a
        /// lost path for a wrong one.
        ///
        /// There is a real backward form for this constraint -- a backward label would have to
        /// carry "which required values are still outstanding", counting down the way a threshold
        /// resource does -- but that is a different representation, not a different rule, and it
        /// is not what this class stores. Until it does, refusing at setup is the honest answer.
        ///
        /// **Nothing required anywhere** is still @c AlwaysTrue: a function that constrains
        /// nothing must constrain the join no more than it constrains an extension. The check is
        /// @ref constrains_something_ in both directions, so an inert function is inert whichever
        /// way it points.
        ///
        /// The asymmetry is not obvious from the class name, which is why it is spelled out here.
        ///
        /// @note The runtime branch survives and stops costing anything. It now selects between
        ///       *call my own body* and *free short circuit*, rather than between two rules a
        ///       third party (@c Resource) has to interpret. Keeping the @c AlwaysTrue arm
        ///       matters: without it a required-values model would pay a virtual call per
        ///       component per candidate pair just to return true.
        ///
        /// **Nothing forbidden anywhere**: also @c AlwaysTrue, and this arm is not cosmetic.
        /// Disjointness is a restriction the *extension* does not impose: it rejects two halves
        /// that share a node even when revisiting that node is legal. A function that forbids
        /// nothing constrains nothing, and must constrain the join no more than it constrains an
        /// extension -- otherwise a component documented as inert makes the bounded search return
        /// a worse answer than the unbounded one, which is exactly what it did (review finding D6:
        /// -51.95 against -80.02 on a cyclic instance, reported `complete`).
        ///
        /// @return @c AlwaysTrue when nothing is constrained anywhere, @c Custom when every node
        ///         forbids exactly itself, and @c Unspecified otherwise -- required values, or
        ///         forbidden values that are not the node's own.
        [[nodiscard]] MergeRule merge_rule() const override {
            if (!constrains_something_) {
                return MergeRule::AlwaysTrue;
            }
            if (!forbidden_) {
                return MergeRule::Unspecified;
            }
            return self_forbidden_only_ ? MergeRule::Custom : MergeRule::Unspecified;
        }

        /// @brief The ng-route condition asks about the node the label is standing on, so it needs
        ///        a memory that excludes it.
        ///
        /// True exactly when @ref merge_rule answers @c Custom, i.e. when this function forbids
        /// each node at itself. That test is `v in my own memory`, and reading it backwards needs
        /// the memory at `v` to exclude `v` -- which is what @c BackwardKind::NodeMirror
        /// guarantees and @c BackwardKind::Mirror does not. Paired with the latter the backward
        /// search dies after its seed; see @c FeasibilityFunction::requires_node_identity_mirror
        /// and check 7 in @c backward_kind.hpp.
        ///
        /// The other two configurations answer @c false because they are refused already: a
        /// required-values function and a non-self forbidden set declare @c Unspecified, and an
        /// inert one declares @c AlwaysTrue and constrains nothing in either direction.
        ///
        /// @return @c true when this function carries the ng-route condition.
        [[nodiscard]] bool requires_node_identity_mirror() const override {
            return forbidden_ && constrains_something_ && self_forbidden_only_;
        }

    private:
        std::shared_ptr<const std::map<size_t, std::set<ValueType>>> values_by_node_id_;
        ContainerResourceType values_;
        bool forbidden_;     // values are forbidden or required
        bool empty_ = true;  // to avoid checking intersection if no values to check

        /// @brief Whether any node forbids anything. Whole-function, not per node; see
        ///        @ref merge_rule.
        bool constrains_something_ = false;

        /// @brief Whether every constraining entry forbids exactly its own node -- the ng-route
        ///        condition, and the precondition @c DisjointMergeForm needs. See @ref merge_rule.
        ///
        /// Whole-function rather than per node, for the reason @ref constrains_something_ is: the
        /// question the merge rule answers is about the model, and `merge_rule()` is cached per
        /// node resource at bind time, so a per-node answer would make one model's components
        /// disagree about their own rule.
        bool self_forbidden_only_ = false;

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

/// A container feasibility function has no scalar bound, so it never seeds a backward label.
template <typename R, typename V>
struct BackSeedEndOf<IntersectionFeasibilityFunction<R, V>> {
        static constexpr BackSeedEnd value = BackSeedEnd::Never;
};

}  // namespace rcspp
