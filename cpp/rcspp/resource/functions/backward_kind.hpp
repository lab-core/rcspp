// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

namespace rcspp {

/// @brief How a resource's backward extension relates to its forward one.
///
/// Declared per extension function via @c ExtensionFunction::backward_kind(). A bidirectional
/// solve refuses to start on any component still reporting @c Unspecified. Forward-only solves
/// never read it.
///
/// **Why this lives in its own header.** The kind is declared by the *extension* function but is
/// also the fact the *feasibility* function needs in order to pick its merge test: a bound-style
/// backward value (@c Threshold) is compared against the forward value, while an accumulating one
/// (@c Accumulate) has to be added to it. `FeasibilityFunction` therefore stores the kind too --
/// see @c FeasibilityFunction::set_backward_kind -- and neither header should have to include the
/// other to name the enum. One shared vocabulary header is the alternative to a dependency edge
/// that points the wrong way.
enum class BackwardKind {
    Unspecified,  ///< not declared -- a bidirectional solve will refuse to run
    Accumulate,   ///< no bound; extend_back == extend (e.g. cost)
    Threshold,    ///< stores a deadline/ceiling; extend_back inverts extend and clamps
    Mirror,       ///< a container accumulating the ARC VALUES of its own half; extend_back ==
                  ///< extend, because the arc value is direction-independent
    NodeMirror,   ///< a container accumulating NODE IDENTITIES, read off the arc's endpoints;
                  ///< extend_back is the same formula with origin and destination swapped
};

/// @brief Whether a kind means "a container carrying the nodes of its own half".
///
/// Both container kinds keep the forward dominance order and neither can serve as the half-way
/// clock, so the places that only care about "is this a set rather than a number" ask this rather
/// than naming both.
///
/// @param kind The kind to test.
/// @return @c true for @c Mirror and @c NodeMirror.
[[nodiscard]] constexpr bool is_container_kind(BackwardKind kind) {
    return kind == BackwardKind::Mirror || kind == BackwardKind::NodeMirror;
}

// ─────────────────────────────────────────────────────────────────────────────────────────────
// WHY THE TWO CONTAINER KINDS ARE SEPARATE
// ─────────────────────────────────────────────────────────────────────────────────────────────
//
// They used to be one value, and the distinction lived only in prose on `NodeMirrorForm`. The
// difference is *where the container's elements come from*, and it decides whether the memory a
// label carries at a node includes that node:
//
//   Mirror      the elements are the ARC'S VALUE, which is the same object in both directions.
//               `UnionExtensionFunction` with the usual {origin} payload therefore holds, at `v`,
//               the nodes strictly BEFORE `v` going forward and `v` ITSELF plus the nodes after
//               it going backward. Nothing is swapped, because there is nothing to swap.
//
//   NodeMirror  the elements are read off the arc's ENDPOINTS, and `NodeMirrorForm` swaps which
//               endpoint is "the node being left" per direction. The memory at `v` excludes `v`
//               in both directions, which is what makes the two halves comparable at a join.
//
// The consequence is not cosmetic. A feasibility function that asks "is the node I am sitting on
// already in my memory" -- `IntersectionFeasibilityFunction` with the ng-route condition -- has a
// backward reading only under `NodeMirror`. Under `Mirror` the backward label arrives at `v`
// already holding `v`, so every backward extension is rejected by the node it lands on, the
// backward search dies after its seed, and with the half-way bound on the solve returns NOTHING
// and reports COMPLETE. Check 7 below is what catches that pairing.
//
// ─────────────────────────────────────────────────────────────────────────────────────────────
// THE BACKWARD COHERENCE CHECKS, IN ONE PLACE
// ─────────────────────────────────────────────────────────────────────────────────────────────
//
// Seven checks now guard the same question -- "do this component's four function objects agree
// about what happens backwards?" -- across three files and two moments. They grew one at a time,
// from three separate reviews, and no single site could see the others. This is the map. Anything
// added to the family belongs in this list too.
//
// **What is derived, and from what.** The extension function's `backward_kind()` is the single
// source of truth. `ResourceGraph::add_resource` reads it once and pushes it into the other two
// objects, so they cannot hold a different opinion:
//
//   ExtensionFunction::backward_kind()          <- declared by the author, the only input
//        |
//        +-> DominanceFunction::backward_reversed_   (== Threshold: a later deadline is looser)
//        +-> FeasibilityFunction::backward_kind_     (read by merge_rule(), see MinMax)
//
// Two further facts are declared independently and cannot be derived, both on the *feasibility*
// side, which is why an incoherent pairing is exactly a disagreement between the two sides:
//
//   `BackSeedEndOf<Feas>`                       a compile-time trait saying where the feasibility
//                                               function's backward seed sits.
//   `FeasibilityFunction::                      a runtime declaration saying the function's test
//     requires_node_identity_mirror()`          only has a backward reading when the memory
//                                               excludes the node it sits on. Read by check 7.
//
// **Compile time**, in `ResourceGraph`'s typed `add_resource` overload. Reached only from C++,
// and only when the four objects are constructed inline; the Python bindings always take the
// erased overload, so these are an early warning, never the only line of defence:
//
//   1. `backward_kind_of_v<Ext> != Unspecified`     -- the author declared nothing.
//   2. `!(Accumulate && BackSeedEnd::Ceiling)`      -- an accumulation seeded at a ceiling. The
//                                                      type-level form of check 6.
//
// **Setup**, in `BidirectionalDominanceAlgorithm::describe_problem`, before the first label. This
// is the complete set: every model reaches it, whichever overload built it.
//
//   3. `kind == Unspecified`                        -- as 1, for models the typed overload never
//                                                      saw.
//   4. `merge_rule() == Unspecified`                -- the feasibility function declared no join
//                                                      test, or declared that it has no valid
//                                                      backward form. Three functions take the
//                                                      second route deliberately; each says so in
//                                                      its own `merge_rule()` doc.
//   5. `Accumulate && rule == DominanceOrder`       -- a *bound's* merge test declared against an
//                                                      accumulation, which compares a prefix
//                                                      against a suffix. The only one of the six
//                                                      whose absence lets a join ACCEPT an
//                                                      infeasible splice rather than lose a path,
//                                                      and acceptances are never replayed.
//   6. `Accumulate && seeds_itself_out_of_range()`  -- as 2, asked of the runtime value rather
//                                                      than the type, so it also catches a
//                                                      feasibility function whose seed end is a
//                                                      constructor argument.
//   7. `requires_node_identity_mirror()             -- a feasibility function that asks about the
//       && kind != NodeMirror`                        node it sits on, paired with a memory that
//                                                     does not exclude that node. See the section
//                                                     above. Like 5 it is a disagreement between
//                                                     two independently legal declarations; unlike
//                                                     5 its failure is a backward search that
//                                                     silently finds nothing rather than a join
//                                                     that accepts too much.
//
// **Why 2 and 6 both exist, and are not redundant.** 2 is a `static_assert` and 6 is a runtime
// probe, so 2 fires earlier and with a better message where it can fire at all -- but it can only
// read the *type*. `MinMaxFeasibilityFunction` picks its seed end from a constructor argument, so
// its `BackSeedEndOf` is `Unknown` on purpose and only 6 catches it. Going the other way, 6 asks
// "is this seed strictly worse than the default state", which is sharper than "is it a ceiling",
// so neither subsumes the other.
//
// **Why 5 is nearly unreachable and stays anyway.** `MinMaxFeasibilityFunction::merge_rule()`
// reads `backward_kind_` and answers `Custom` under an accumulation, so the one class that used
// to trip 5 no longer can. 5 remains because the hazard belongs to the *pairing*, not to that
// class: any feasibility function that declares `DominanceOrder` without consulting its extension
// function has it.

}  // namespace rcspp
