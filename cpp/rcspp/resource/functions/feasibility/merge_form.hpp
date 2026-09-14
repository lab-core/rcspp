// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include "rcspp/resource/functions/feasibility/feasibility_function.hpp"

namespace rcspp {

/// @brief Supplies the disjointness merge test for a container resource.
///
/// A forward half's remembered set and a backward half's must not overlap: the backward label
/// stores the threshold *complemented* -- the nodes seen rather than the nodes still allowed --
/// so "f is within the threshold" is `f subset of complement(V_b)`, which is exactly
/// `f intersect V_b == empty`.
///
/// **The first precondition** is that the model already forbids revisiting the nodes it
/// remembers. A label's remembered set is then a subset of the nodes on its own half, and an
/// *elementary* path's two halves are node-disjoint, so disjointness never rejects an elementary
/// path. On a model that permits revisits it rejects plenty -- two halves that legally share a node
/// are refused at the join while the same walk is perfectly reachable by extension -- which is why
/// @c IntersectionFeasibilityFunction declares @c AlwaysTrue when its forbidden sets are empty,
/// i.e. when it is not that model after all.
///
/// **The second precondition is load-bearing in the same way.** Disjointness is exact exactly
/// when **the stored set is the memory the label will carry out of this node** -- i.e. when
/// nothing further will be filtered out of it before the suffix sees it. Two ways to satisfy
/// that:
///
///  - the memory never forgets (a plain visited set, `UnionExtensionFunction`); or
///  - the memory forgets, but the narrowing has *already been applied* by the time the label
///    arrives. That is what @c NgPathExtensionFunction now does: it stores
///    `(memory ∪ {node left}) ∩ ng(node arrived)` rather than the pre-narrowing set.
///
/// Get that wrong and the join compares a one-step-stale set against a current one, so two halves
/// that share a node the merge node was about to forget are refused -- while the same walk is
/// perfectly reachable by extension. The forward search then accepts a path the join will not, and
/// `bidirectional` and `simple` return different optima on one model, with `half_way_point`
/// deciding how different. That is not a tighter relaxation: the strictness applies only to paths
/// that cross `H`, so it cannot be stated as a model at all.
///
/// Note what is *not* wrong with it, because the intuition here matters: a rejected pair always
/// contains a cycle, and disjointness never rejects an *elementary* path, so a column-generation
/// bound built on such a pricer stays valid. What is lost is that the answer stops being a property
/// of the model and becomes a property of the algorithm and its tuning.
///
/// So: **declare this rule only where one of the two conditions above holds**, and prefer the
/// second -- storing the already-narrowed memory costs nothing at the join, makes the stored set a
/// sufficient statistic, and therefore strengthens dominance as well.
///
/// **Why this is a template rather than an arm of `Resource::can_be_merged`.** As an arm it had
/// to compile for every `Resource<R>` instantiation, including the scalar cost resource, which
/// has no `intersects()`. That forced a concept to guard it, a `logic_error` in the `else`, a
/// setup-time complaint to pre-empt the throw, and a test to cover it -- four artefacts to
/// support one arm. Here the body is instantiated only where an author asks for it, so
/// `intersects()` is required only where it exists, and a scalar resource cannot declare
/// disjointness at all.
///
/// The general lesson, since it generalises past this class: a declaration only has to be a
/// *value* interpretable by a stranger because its body lives somewhere else. Where the body
/// comes home, the declaration's runtime-ness stops being a constraint on the design.
///
/// @note @c override, not @c final. @c IntersectionFeasibilityFunction narrows @c merge_rule() to
///       short-circuit the required-values case, which is the whole reason this is a form rather
///       than a fixed base. The form's value is that the *body* is supplied, not that it is
///       sealed.
///
/// @tparam R    The container resource type. Must expose `intersects()`.
/// @tparam Base The base to insert above -- @c FeasibilityFunction<R> in every current use.
template <typename R, typename Base>
class DisjointMergeForm : public Base {
    public:
        /// @brief Two halves may be merged only when their remembered sets do not overlap.
        ///
        /// @param resource      The forward label's value.
        /// @param back_resource The backward label's value.
        /// @return @c true when the two sets are disjoint.
        [[nodiscard]] auto can_be_merged(const R& resource,
                                         const R& back_resource) -> bool override {
            return !resource.intersects(back_resource.get_value());
        }

        /// @brief The merge test lives on this object now, so the rule is @c Custom.
        ///
        /// @return @c MergeRule::Custom.
        [[nodiscard]] MergeRule merge_rule() const override { return MergeRule::Custom; }
};

}  // namespace rcspp
