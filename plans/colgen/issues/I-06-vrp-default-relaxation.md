# question(vrp): which route relaxation should the VRP example and its benchmarks use by default?

## Facts

- `examples/cpp/vrp/vrp.hpp:24-34`: `enum class RouteRelaxation { None, NgPath, Elementary }` and `inline constexpr RouteRelaxation kRouteRelaxation = RouteRelaxation::None;`. Changing it means editing the source and rebuilding.
  - `None`: routes may revisit customers (cycles allowed).
  - `NgPath`: `presets::add_ng_path_resource`, with 8 nearest neighbours by default (`VRP(Instance, size_t ng_neighborhood_size = 8)`).
  - `Elementary`: `presets::add_elementary_resource`; documented as "very slow even on small instances".
- `rcspp-vrp-benchmark` and `rcspp-vrp-benchmark-large` therefore price **with cycles allowed**. `rcspp-ng-cg-comparison` `static_assert`s `NgPath`, so it needs a different build from the benchmarks.
- When Boost is found (`RCSPP_VRP_HAS_BOOST`), every iteration compares rcspp's best reduced cost with Boost's (`examples/cpp/vrp/vrp.cpp`, ~203-230) and logs an `ERROR` on mismatch. As far as I can see, the Boost subproblem has no ng memory. Under `NgPath` the two solve different problems, so this check would report false mismatches. That is probably why `None` is the default.

## Consequences

- **Weak bounds.** The run of 2026-10-20 gave RC101 an LP of 1572.74, 2.9 % below its BKS (1619.8).
- **Not comparable with the literature,** whose LP values use elementary or ng routes (and 1-decimal distances, see #54).
- **Benchmarks measure a different problem.** The algorithms are timed on cycle-allowed pricing, which is cheaper per solve than the ng-route pricing real branch-and-price solvers use.
- **One binary per relaxation,** so one run can't compare relaxations.

## Options

1. **Switch the default to `NgPath` (size 8)** and run the Boost cross-check only under `None`.
2. **Make the relaxation a runtime option.** The VRP constructor already says that an ng size of 0 "keeps the memory but makes it inert". With the ng resource always registered:
   - size 0 behaves like `None`;
   - size *n* (every customer) behaves like `Elementary`, since an ng-route with full neighbourhoods is elementary;
   - one build covers all three, at the cost of the inert memory resource in `None` runs (to measure).

   The Boost cross-check would run only at size 0.
3. **Keep `None`**, but print the relaxation in every benchmark output and in the docs.

## Questions for maintainers

- Which relaxation should published benchmark numbers use: `None` (current), ng-8, or a per-instance ng size?
- Is the Boost cross-check still needed, given the equivalence tests added with the bidirectional stack?

Suggested answer: option 2, default ng-8, cross-check only at size 0, relaxation printed in every result.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
