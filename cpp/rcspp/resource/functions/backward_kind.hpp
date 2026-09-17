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
    Mirror,       ///< stores a set seen on its own half; same formula, origin/destination swapped
};

// ─────────────────────────────────────────────────────────────────────────────────────────────
// THE BACKWARD COHERENCE CHECKS, IN ONE PLACE
// ─────────────────────────────────────────────────────────────────────────────────────────────
//
// Six checks now guard the same question -- "do this component's four function objects agree
// about what happens backwards?" -- across three files and two moments. They grew one at a time,
// from two separate reviews, and no single site could see the others. This is the map. Anything
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
// A fourth fact is declared independently and cannot be derived: `BackSeedEndOf<Feas>`, a
// compile-time trait saying where the feasibility function's backward seed sits. It describes the
// *feasibility* side where `backward_kind` describes the *extension* side, which is why there are
// two and not one -- an incoherent pairing is exactly a disagreement between them.
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
