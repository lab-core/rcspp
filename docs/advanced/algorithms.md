---
title: Algorithms
---

# Algorithms

## Choosing an algorithm

| Algorithm | Optimal | Speed | When to use |
|---|---|---|---|
| `Simple` | ✓ | medium | Default; correct answer needed |
| `Pushing` | ✓ | medium–fast | Dense graphs; pushing dominance forward |
| `Pulling` | ✓ | medium–fast | Dense graphs; pulling dominance from in-arcs |
| `Bidirectional` | ✓ | fast–slow | Long routes with a tight, monotone clock |
| `AStar` | ✓ | fast–slow | Good heuristic available |
| `Greedy` | ✗ | fast | Quick feasible solution; large graphs |
| `Tabu` | ✗ | medium | Metaheuristic diversity |
| `Diversification` | ✗ | configurable | Generate a diverse pool of solutions |

## What every algorithm returns

A path starts at a **source** and ends at a **sink**, with **neither a source nor a sink strictly
inside it**. The graph refuses an arc that no path could use: `add_arc` throws
`std::invalid_argument` for an arc into a source or out of a sink (in Python, a `ValueError` when the
buffered arcs are sent to the graph). The algorithms keep the rule as well: none continues past a
sink it reaches, and none extends a path into a source. A walk that returns to the depot and leaves
again is two routes; priced as one column, with the vehicle-count row counted once, a
set-partitioning master would buy it and be wrong.

:::{admonition} Changed
:class: note

The rule used to hold only in part. Every algorithm could pass through a *source*, and `pulling`
and `greedy` -- with the tabu and diversification searches built on the same dive -- could also
continue past a sink. With sources `0` and `1`, sink `2`, and arcs `0 → 1` (−5), `1 → 2` (−1),
`0 → 2` (0), the optimum was −6 through `0 1 2`. That graph is now refused when it is built, at
`0 → 1`; without that arc the optimum is −1 through `1 2`. A route that genuinely continues through
a depot is two routes: give the depot separate source and sink copies, which is what the VRP example
does.
:::

## `Simple` (default)

Classic forward label-setting.  Processes nodes in topological order and
propagates labels; dominated labels are discarded immediately.

```python
result = rg.solve()                        # algorithm="simple" by default
result = rg.solve(algorithm="simple")
```

## `Pulling`

Forward label-setting like `Simple`, but each node *gathers* labels through its
in-arcs instead of each label being *pushed* through out-arcs.  The labels still
travel source → sink; this is not a bidirectional search.

## `Bidirectional`

Searches forward from the sources and backward from the sinks, stopping each
direction half-way along one designated resource, and joins the surviving halves
at the node where that resource crosses the middle.  Because dominance is
checked within each half rather than over whole paths, the number of labels grows
with half the path length instead of all of it.

```python
from rcspp.graph import AlgorithmParams

p = AlgorithmParams()
p.critical_resource_index = 1     # slot 1 is the clock (time), not the cost
p.half_way_point          = 500.0 # H; the resource range is taken as [0, 2H]

result = rg.solve(algorithm="bidirectional", params=p)
```

`half_way_point` has no useful default: **0 turns the bound off**, it does not derive one.  The
algorithm has no way to read a feasibility function's upper bound, so `H` is the only number it
has to go on — it takes the range to be `[0, 2H]` rather than the other way round.  With the bound
off both searches run to completion and the join considers every pair, which is correct but
*slower* than a forward solve rather than faster, since it is a forward search plus a backward
search plus a join.  That is the supported way to ask for an unbounded run, and
`result.bounded_by_half_way` tells you which of the two you got.  A solve that wants the speed-up
sets `H` explicitly, to roughly half the clock's range.

Because the default gets you the slow case without asking for it, a solve that runs with the
bound off — for this reason or any of those in the next section — logs a warning saying why.  It
does so once per reason per process, since the Python bindings build a new algorithm for every
solve and a pricing loop would otherwise repeat it on every iteration; later solves log the same
sentence at debug level.  From C++, `SolveResult::half_way_off_reason` holds it for every solve,
empty when the bound was in force.

### The critical resource

One resource acts as the **clock** that says where "half-way" is.  It must be:

- **monotone** — never decreasing along an arc, so its value crosses `H` exactly
  once along any path;
- **bounded**, with a known finite range;
- **a threshold backwards** — its backward form must carry a *ceiling* on the
  forward scale (the latest arrival still admissible here), not the consumption
  from here to the sink.  `TimeWindowExtensionFunction` and
  `CapacityExtensionFunction` are thresholds; `AdditionExtensionFunction` is not.
- **inside the dominance order, increasing** — the resource's `DominanceFunction` must say that a
  smaller value dominates a larger one. `ValueDominanceFunction` does; `TrivialDominanceFunction`
  does not. Without it a dominating label can sit *above* the half-way point while the label it
  evicted sat below, and the path through the evicted label is lost.

Cost is never a valid clock: reduced costs go negative during column generation,
which breaks monotonicity.

**The answer matches a forward search exactly only when the clock's arithmetic is exact**: an
integer clock, or real values whose sums never round.  The backward search subtracts an arc's
time from a deadline, and in floating point `b - a` is not always the inverse of `t + a`.  So a
path that reaches a deadline exactly can be lost: arcs of 0.1, 0.1 and 3.9 against a deadline of
`0.2 + 3.9` returned nothing where `simple` finds the path.  With preprocessing off, a path that
misses a deadline by one rounding can also be accepted: 0.1 + 0.2 against 0.3.  If paths land on
their deadlines, as they do with one-decimal times and integer service times, scale the times to
integers.

If the chosen resource fails any of these, **the half-way bound is switched off**
and a `LOG_DEBUG` line says why.  The solve stays correct — both directions simply
run to completion and every pair is considered — it is only slower.  A *wrong*
bound would be far worse: it can discard a valid path in both directions and then
report `complete`.

`critical_resource_index` is an index within the **cost resource type's** slot, so
the clock must share that type (a real-valued time alongside a real-valued cost is
the usual case).  A model whose clock is an `int` resource while its cost is
`real` needs the C++ API, where the clock's type is the last template parameter of `solve` and
`create_algorithm`, after the cost's, and defaults to it:

```cpp
using Comp = rcspp::ResourceTypeComposition<rcspp::RealResource, rcspp::IntResource>;
using LC = rcspp::LabelList<Comp>;
rcspp::AlgorithmParams<LC> params;
params.direction = rcspp::SearchDirection::Bidirectional;
params.critical_resource_index = 0;  // the first int component
params.half_way_point = 500;
// cost type RealResource, clock type IntResource
auto result = graph.solve<rcspp::SimpleDominanceAlgorithm, rcspp::RealResource, LC,
                          rcspp::IntResource>(std::move(params));
```

### What the model must declare

Every resource in the model must declare how it behaves backwards, and how two halves join.  A
bidirectional solve runs the model checks first, `BackwardExtensionCheck` and `JoinCheck`, and
refuses a model that fails them **before the first label is created**, with a `ModelRefused` that
lists every problem:

```text
a bidirectional search cannot run on this model:
  - component 1: arc 1 -> 2 consumes -3.000000, but its backward reading assumes no arc
    lowers the value: a backward label carries only a ceiling, so a path dipping below the
    floor would be accepted; loads must be non-negative
```

[Model checks](#model-checks) lists the twenty complaints, what each means and what to do, and
`ResourceGraph::check_model(SearchDirection::Bidirectional)` runs the checks without solving.

A **capacity written as an addition**, `AdditionExtensionFunction` with
`MinMaxFeasibilityFunction(0, capacity)`, is not one of them: `MinMaxFeasibilityFunction` picks its
join test from the extension it is paired with.  Under an addition a backward label starts at
the empty suffix and counts its load *up* from zero, and the join tests
`prefix + suffix <= capacity`.  That is exact while loads are non-negative, which the checks
verify.  `CapacityExtensionFunction` is the other way to write it, and the one that can also be the
half-way clock:

```python
from rcspp.resource import CapacityExtensionFunction, MinMaxFeasibilityFunction

rg.add_real_resource(
    CapacityExtensionFunction(capacity),         # additive forward, threshold backward
    MinMaxFeasibilityFunction(0.0, capacity),  # the same capacity
    TrivialCostFunction(),
    ValueDominanceFunction(),
)
```

`CapacityExtensionFunction` is available for the signed numerical types (`"real"`
and `"int"`) only — extending backwards subtracts, and on an unsigned type that
wraps to a huge positive value that reads as a very loose bound.

One pairing needs no refusal: a time window's extension with a feasibility function whose floor is
lower, such as `MinMaxFeasibilityFunction(0, cap)`.  A threshold extension marks a backward
deadline below the node's opening time as unmeetable itself, whatever the feasibility function
tests.

Two things the join does *not* take from a component's declarations, so there is nothing to get
wrong.  Its join test compares values, never the dominance function: a relaxed dominance on a
time window — ignoring time in dominance is a common heuristic pricing choice — costs optimality,
as it does forward, and never lets an arrival past the deadline join.  And a threshold extension
clamps its backward label to each node's upper bound **as its own bounds state it**, so those must
be the feasibility function's.  Give both functions one `NodeBounds`, as the presets do:

```cpp
auto caps = rcspp::make_node_bounds(0.0, capacity, per_node_caps);  // {node: {0, cap}}
graph.add_resource<RealResource>(
    std::make_unique<CapacityExtensionFunction<RealResource>>(caps),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(caps),
    std::make_unique<TrivialCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());
```

`TimeWindowExtensionFunction` and `TimeWindowFeasibilityFunction` take a `NodeBounds` of windows
the same way, and every feasibility function that has one returns it from `bounds()`.  The
checks refuse a clamp that differs from the node's bound, in either direction.

#### C++ callers can ask for one of these at compile time

One of the checks can be answered from the *types* alone, and C++ exposes it as a trait,
`rcspp::backward_coherent_v<Ext, Feas>` (in `rcspp/resource/presets.hpp`): the extension
function declares a `BackwardKind` other than `Unspecified`.

Every preset asserts it on its own pairing, so an incoherent preset does not compile.  For a
pairing of your own, assert it yourself:

```cpp
static_assert(rcspp::backward_coherent_v<MyExtension, MyFeasibility>);
```

`add_resource` itself does **not** assert it, on purpose.  A model that is never solved
bidirectionally owes the check nothing — a custom extension function with no declared kind
and an unsigned time window are both valid forward-only models — so a hard error there would
reject correct code.

So the trait is an **early warning on a subset**, not a replacement: every backward or
bidirectional solve runs the model checks first, from C++ and from Python alike, with the same
messages and component numbering.

The full inventory — all six checks, which fires where, and why no one of them subsumes
another — is in `cpp/rcspp/resource/functions/backward_kind.hpp`.

### One rule the join imposes, and what it requires of a container resource

A container resource can declare that two halves may only be joined when their remembered sets are
**disjoint**.  `IntersectionFeasibilityFunction` declares a refinement of that when it forbids each
node at itself, which is what the ng-path relaxation means: two halves conflict only on a node both
remember **and** that forbids itself.  A node both remember but that has no forbidden entry is one
the model lets a path revisit, so it does not block the join.

That test needs two things, and the second one is the one people miss.

#### The forbidden sets have to be `forbidden(v) = {v}`

`is_feasible` here asks *what has this label collected so far* — a predicate on a **prefix** — and
`is_back_feasible` is inherited unchanged, so a backward label's **suffix** is asked it too. Only
the self-forbidden form survives that: forwards it reads "the prefix already visited `v`",
backwards "the suffix visits `v` again", and the cross case is exactly what disjointness tests.

Any other entry, `forbidden(u) = {w}` for some other node `w`, is refused by the model checks.
It would fail by **accepting**: a forward half can collect `w` before the meeting node while the
suffix passes through `u`, and nothing checks it — the backward label never sees `w`, and the join
test does not either, because `w` is not on the suffix.  The solve would return a path the model
forbids, with a `complete` status.

A node with **no** entry is not refused, and needs no refusal.  Both halves may remember it — an ng
neighbourhood that mentions a charging station the model lets a route revisit, say — and the join
test ignores it, since it forbids nothing.  Plain disjointness would refuse that splice, and
`bidirectional` would answer a stricter question than `simple`.

Forward-only solves are unaffected by all of this — the restriction is on the backward reading, not
on the model.

#### The memory has to come from the arc's endpoints, not its value

`forbidden(v) = {v}` needs the memory at `v` to exclude `v` **in both directions**, and that
depends on where the container's elements come from:

- `NgPathExtensionFunction` reads them off the arc's *endpoints* and swaps which endpoint is "the
  node being left" per direction — `BackwardKind::ArcEndpoints`. The memory at `v` is the nodes
  before it going forward and the nodes after it going backward. Correct.
- `UnionExtensionFunction`, `IntersectionExtensionFunction` and `SubtractExtensionFunction`
  accumulate the arc's *value*, which is the same object both ways — `BackwardKind::ArcValue`. With
  the usual `{origin}` payload the backward memory at `v` contains `v` itself, so the feasibility
  test rejects every backward label the moment it is created and the backward search keeps nothing
  but its seed.

The second is the obvious way to write an elementary path and it fails silently, so the model
checks refuse that pairing for a backward or bidirectional solve. Use
`presets::add_elementary_resource`.

#### The stored set has to be the memory the label carries out of its node

Because at the join a forward half and a backward half compare their memories at the node where
they meet, and the forward half's must already have been filtered by that node. Two ways to satisfy
it:

- the memory never forgets — a plain visited set built with `UnionExtensionFunction`; or
- the memory forgets, but the narrowing has already been applied on arrival. That is what
  `NgPathExtensionFunction` does: it stores `(memory ∪ {node left}) ∩ ng(node arrived)`.

If neither holds, the join compares a one-step-stale set against a current one and refuses splices
the model permits. The symptom is specific and worth recognising: **`bidirectional` returns a worse
optimum than `simple` on the same graph, and a worse one with the half-way bound on than with it
off** — because with the bound off most paths are found end-to-end and never reach the join. Every
path refused that way contains a cycle and no *elementary* route is ever lost, so a
column-generation bound stays valid; what is lost is that the answer stops depending only on the
model.

So if you attach a container resource of your own to a model that permits revisits, either make its
extension narrow on arrival as above, or give it a `join_rule()` of `AlwaysTrue`; do not give it
forbidden sets it does not mean.

The simplest way to get both conditions right is `presets::add_ng_path_resource`, which pairs
`NgPathExtensionFunction` with an `IntersectionFeasibilityFunction` and derives
`forbidden_by_node[v] = {v}` itself, at every node the neighborhoods mention.

`SizeFeasibilityFunction` shows the other half of the same question. Its join test counts
`|forward ∪ backward|` — the union, because two halves that meet have both collected whatever they
share and the joined path holds it once — against the cap. For a container that only grows, that
count is the one at the sink and bounds every set earlier on the path, so with a single cap the
test is exact. That is why per-node caps are refused rather than joined: the join would have to
know which caps the suffix passed under, and two values do not say.

Every join rule in the library is exact, and the join trusts both its "yes" and its "no". If you
write a join rule that cannot decide its question exactly from two values, declare
`JoinRule::Unspecified` so that a bidirectional solve refuses, rather than over- or
under-rejecting quietly.

### Parameters that behave differently here

| Parameter | Under `simple` / `pushing` / `pulling` / `astar` | Under `bidirectional` |
|---|---|---|
| `return_dominated_solutions` | a path reaching a sink is recorded immediately, so paths later dominated are still returned | the same, in both directions: a forward path reaching a sink and a backward path reaching a source are both recorded when they are found.  Joined paths are **not** filtered by dominance at the sink, whatever the flag says: see below |
| `stop_after_X_solutions` | stops the search once that many solutions exist | stops the *search* the same way, but never cuts the join short: the join still considers every pair, splices only the cheapest `stop_after_X_solutions` distinct joined paths, and the result list is resized afterwards, so a `complete` status still means the search was exhaustive |
| `prune_based_on_upper_bound_` | drops a label whose own cost is at or above the incumbent — valid for a complete path, not for a frontier | drops a half whose cost *plus a lower bound on its completion* is at or above the incumbent, and drops a join that is no better than the incumbent. When the reduced-cost graph has a negative cycle there is no finite lower bound, and no half is dropped for its cost. With it off **and a finite `upper_bound`**, every join that bound admits is returned — which is what a pricing pool wants. With no `upper_bound` at all the join keeps pruning against the incumbent whatever the flag says: see below |
| `timeout_s`, the memory limit | stop the search; whatever was found is returned | stop the search, then **still run the join**, with the incumbent cutoff forced on. With the half-way bound in force few forward labels reach a sink, so without the join a truncated solve would return almost nothing, and a pricing loop would stall on it. The join hands back the improving paths the two halves already hold — both halves, since the two searches are interleaved by the work each has done, so a stop finds each partly built; the status still says the run was cut short. `join_after_early_stop = false` skips the join instead, as before this was added |
| `should_stop` | stops the search | stops the search **and skips the join**: an interrupt means stop now |
| `join_column_budget` | ignored | the join keeps only its cheapest this many paths and never splices a pair that cannot enter them. Unlike `stop_after_X_solutions` it **never stops the search**, so `complete` still means exhaustive, and since the cheapest path is always kept the optimum is unchanged. The column *set* becomes "the K cheapest joined paths". The join uses the tighter of the two budgets |
| `max_join_pairs` | ignored | the join stops after this many join-rule questions (`join_pairs_tested`). What it found is returned, but the result is no longer a proof: `join_truncated` is set and `could_be_non_optimal()` is true |

Why the last row depends on `upper_bound` being finite: a solve that supplies none has expressed no
filter, and returning every admissible pair there is nobody's question.  Measured on the full C201
instance, doing so produced **3 065 288 solutions in 12.2 s** where the forward search finished the
same optimum in 0.68 s — while the bidirectional *search* extended 1.46× **fewer** labels.  On
RC201 the same run reached 9.3 GB resident.  None of that is search cost; it is recording and
de-duplicating the set.  So the incumbent cutoff stays on when there is no bound, and the flag is
honoured where it means something.  The same measurement is why the join after an early stop
always prunes against the incumbent, and why `max_join_pairs` exists: it caps the join's work in a
unit that, unlike seconds, does not depend on the machine.

The bidirectional solution *set* is not the forward search's, and with a finite bound and pruning
off it is usually **larger**.  The join pairs every forward half with every backward half the
bound admits, and a pair that would be dominated at the sink is still returned: two halves are
each non-dominated where they meet, not as a whole path.  On Solomon pricing graphs at half the
horizon, with a uniform dual and pruning off, it returned 12 to 19 times the forward search's
columns.  Every one of them is feasible at its stated cost, and the optimum is the same.  If the
master should see only non-dominated columns, use a forward algorithm.  To cap the count, use
`join_column_budget`: the join then keeps only its cheapest paths, without building the rest, and
the search stays exhaustive.  `stop_after_X_solutions` also caps the join, but it stops the search
as well.

### Where the two halves are paired

How the join forms a pair belongs to the same discussion, because it is the other thing that decides
how many columns come back.

The forward search has already built the label that crosses `H`.  It extends a half whose clock is
at or below `H`, the result lands past it, and that result is then stored and dropped without being
extended again.  Those **boundary labels** are paired with the backward labels sitting at the same
node.  Nothing is rebuilt: the boundary label *is* the crossing, it is feasible because an
infeasible label is never stored, and the join arc is already inside its cost.

The alternative — rebuilding, for each arc `(u, v)`, a forward half at `u` across the arc and
comparing it with a backward half at `v` — was implemented, measured against this one, and removed.
Both always returned the same optimum.  What differed is which forward halves are eligible: a
boundary label had to survive dominance at the meeting node, while a rebuilt one never faced that
filter, so the arc-based form also paired halves whose prefix was dominated there.  The optimum
cannot be among those — a dominated prefix completes no better than the prefix that beat it — and
the *forward* algorithms would not have returned them either, which is why the surviving join is the
one that agrees with `simple` about which prefixes count.  It also walks 31–47× fewer candidate
pairs.

**Measured against a forward-only solve**, which is the comparison that matters if you are switching
from `simple`: on 96 generated configurations the forward search returned 1 608 columns and both
joins reproduced the same 1 601 of them; on five Solomon instances forward returned 2 770 and both
reproduced the same 1 767.  The join loses no column a forward-only solve would have produced.  What
it misses relative to forward is the **half-way bound**, not the join — the bound stops forward
labels before they reach a sink.

One caveat, and it only applies in modes that are already inexact.  When the per-node extension
quota binds — `num_labels_to_extend_by_node` (truncated labeling), or `on_memory_pressure` tightening
it automatically — a forward label can be stored and never grown, so it never produces the boundary
label the join reads, and that pair is lost.  At the default quota of "unlimited" this never arises.

### Presets: declaring a resource in one call

Most resources are one of five shapes, and for those the four function objects can be
constructed for you, so they cannot disagree:

| Preset | What it is | C++ | Python |
|---|---|---|---|
| cost | unbounded accumulation; the objective | `presets::add_cost_resource<R>` | `presets.add_cost_resource` |
| window | per-node `[earliest, latest]`; a threshold | `presets::add_window_resource<R>` | `presets.add_window_resource` |
| capacity | bounded accumulation — capacity, duration | `presets::add_capacity_resource<R>` | `presets.add_capacity_resource` |
| ng-path | memory read from the arc's endpoints + forbidden sets | `presets::add_ng_path_resource<R>` | *not available* |
| elementary | ng-path with nothing ever forgotten | `presets::add_elementary_resource<R>` | *not available* |

```cpp
#include "rcspp/rcspp.hpp"

rcspp::presets::add_cost_resource<RealResource>(*graph);
rcspp::presets::add_window_resource<TimeResource>(*graph, windows);   // the map, once
rcspp::presets::add_capacity_resource<DemandResource>(*graph, capacity);
```

```python
from rcspp import presets

presets.add_cost_resource(rg)                      # must come first, and be "real"
presets.add_window_resource(rg, "real", windows)
presets.add_capacity_resource(rg, "int", capacity)
```

`add_capacity_resource` is the one that earns the feature: it is the correct spelling of a bounded
accumulation, and writing it by hand is where the capacity-as-an-addition mistake above comes
from. `add_elementary_resource` earns it the same way — the obvious hand-written elementary path
is a visited set (`UnionExtensionFunction` over arcs carrying `{origin}`), and that is incoherent
backwards for the reason given above, under "One rule the join imposes".

**The four-object `add_resource` form remains normative.** Presets are sugar over it: each one's
doc comment names exactly what it expands to, and a model that needs a flipped dominance, a
non-trivial cost on a capacity, or a floor on a capacity uses the general form. Presets
deliberately do not chase constructor parity.

The C++ capacity preset takes optional per-node capacities, as a map from node id to cap —
`add_capacity_resource<R>(*graph, capacity, per_node)` — and hands them to the feasibility
function, the model's one statement of them; the extension takes its backward clamp from there.
There is no floor, because a backward label on a threshold carries only a ceiling and a minimum
would never be checked on the backward side.

Three asymmetries worth knowing. Python presets take the resource type as an argument
(`"real"`, `"int"`) because there is no template parameter to carry it. The Python capacity
preset takes a uniform capacity only, because `MinMaxFeasibilityFunction` takes a single window
from Python. And Python has **no**
ng-path preset — neither `NgPathExtensionFunction` nor `IntersectionFeasibilityFunction` is
exposed to Python, so an ng-path model cannot be assembled from Python at all today, with or
without a preset.

### When it pays, and when it does not

What decides this is **how many labels dominance has to sift at each node**, not how
long the horizon is and not how tight the windows are.  Dominance compares a new
label against every label held at its node, so halving the set at each node makes
each comparison cheaper as well as making fewer of them — which is why the
wall-clock gain, where there is one, is larger than the reduction in label
extensions.

Measured in a Release build across all six Solomon families, nineteen instances,
every one run to completion and every one agreeing on the optimum (the disabled
`*ExactComparison*` benchmark in `tests/cpp/test_bidirectional_benchmark.hpp` reprints the full
table).  What predicts the
gain is **label pressure** — how many labels the forward search extends — and not
the horizon, the window tightness, or the customer count on their own:

| Forward extensions | Effect |
|---|---|
| under ~13 000 | 0.8× — a consistent small loss; the overhead exceeds the saving |
| ~13 000 – 100 000 | 1.0–4.2× |
| ~100 000 – 1 M | 1.3–6.2×, widening as instances grow |
| over 3 M | **35–41×**, on the two hardest instances in the set |

The last row is the one worth designing for.  Dominance is quadratic in the labels
held per node, so halving the path length pays off super-linearly, and only the
largest instances are big enough to show it: R202_50 goes from 1 820 s to 44 s and
C202_50 from 107 s to 3.0 s, both proving the same optimum as the forward search.

> Label *counts* reproduce across machines and across build configurations; the
> wall-clock ratios do not.  A Debug build is roughly 20× slower than a Release
> one and unevenly so — it flatters bidirectional — so figures measured there read
> higher than these.

So it is **not a default**.  It does not pay when:

- **Few labels survive per node**, whatever the horizon.  A clustered instance with
  tight windows can have a horizon five times longer than another and still hold
  smaller label sets — and there the second set of containers, plus a join pass that
  pairs the labels at every node, costs more than the halving saves.
- **Short routes**, which is the same thing seen from the other side: nothing to halve.
- **No usable clock**, in which case the bound turns itself off and the search becomes
  a forward search plus a backward search plus a join — strictly more work than
  `Simple`.  The result reports it:

  ```python
  result = rg.solve(algorithm="bidirectional", params=p)
  if not result.bounded_by_half_way:
      print("the clock was rejected; this solve was slower than a forward one")
  print(result.number_of_joined_paths, "paths came from the join")
  ```

  `bounded_by_half_way` is worth checking rather than assuming. `number_of_joined_paths`
  tells the two payoff failures apart: zero with a correct answer means the answer came
  from a search reaching a terminal rather than from the join, so `H` is placed such that
  nothing crosses it.

### What a solve reports about itself

Beyond `status`, every `SolveResult` carries diagnostics the status cannot express:

| Field | Reported by | What it says |
|---|---|---|
| `memory_pressure_triggered` | every algorithm | the run was trimmed, so `complete` is not a proof |
| `bounded_by_half_way` | `bidirectional` | whether the half-way bound was in force |
| `number_of_joined_paths` | `bidirectional` | how many complete paths the join produced |
| `forward_labels` | the four exact forward algorithms, and `bidirectional` | labels surviving dominance in the forward containers |
| `backward_labels` | `bidirectional`, and a backward-only search | the same, backwards |
| `dominance_checks` | as above | dominance comparisons performed, both directions |
| `half_way_point_used` | `bidirectional` | the `H` actually applied; `0.0` when the bound was off |
| `join_pairs_tested` | `bidirectional` | pairs the join tested; 0 when it did not run |
| `join_truncated` | `bidirectional` | the join stopped at `max_join_pairs` with a pair left, so `complete` is not a proof |

The heuristics — `greedy`, the tabu searches, `diversification`, `backtracking_dive` — derive from
`Algorithm` directly rather than from the directional base, so they leave the label and check
counts at 0.

Two of these answer questions no single number does.  **`forward_labels / backward_labels`** is the
imbalance the half-way point controls: a ratio far from 1, iteration after iteration of a column
generation, means `H` is placed where one direction does nearly all the work.  And
**`dominance_checks` divided by the surviving label count** is the cost of the dominance rule per
label kept — the quantity a container change has to move to be worth having, and one the label
count alone cannot see, because it cannot distinguish "fewer labels" from "the same labels, sifted
more cheaply".

```python
result = rg.solve(algorithm="bidirectional", params=p)
if result.backward_labels:
    print("imbalance:", result.forward_labels / result.backward_labels)
print("checks per label:",
      result.dominance_checks / max(1, result.forward_labels + result.backward_labels))
```

Two things to know before reading the numbers.

`half_way_point_used` is deliberately not `params.half_way_point`: the bound disables itself when
the critical resource fails validation, and a dynamic half-way point (below) moves `H` between
solves, so the number a caller asked for and the number applied can differ.  Read
`bounded_by_half_way` alongside it to tell "the bound was off" from "`H` really was 0".

And **zero dominance checks is a real answer, not a broken counter.**  A node's container is
consulted *before* the new label is inserted, so the first label to arrive is compared against
nothing.  On a graph where each node receives one label per direction — a line — the count is
legitimately 0.  It becomes non-zero exactly when two partial paths meet at a node, which is also
the only situation in which dominance does any work.

`examples/cpp/bidirectional_cg_main.cpp` prints all of these per pricing solve across a whole
column generation, which is the shape in which they are worth reading.

### A half-way point that moves between solves

A static `H = R/2` can leave one direction doing nearly all the work: on `R201_25` the forward
search kept 2 to 10 times the backward search's labels at every iteration of a column generation.
`HalfWayController` corrects that **between** solves, after RouteOpt's meet-point rule.  After each
solve it reads `forward_labels` and `backward_labels` and:

1. **learns only from a trustworthy solve** — `complete`, untrimmed by memory pressure, not
   truncated, with the bound in force.  Anything else is `Skipped`;
2. **moves away from the heavier side** when `|f − b| / min(f, b)` exceeds a 20 % dead zone:
   `H × 0.8` when forward is heavier (so forward stops earlier), `H × 1.2` when backward is;
3. **pulls `H` back toward the centre** when the counts are balanced but nothing was joined while
   solutions were still found — the answer then came from a search running all the way to a
   terminal, and the split bought nothing;
4. **halves the step whenever the direction reverses**, down to a floor of 2.5 %, so `H` settles
   instead of oscillating; and keeps `H` inside `[0.05, 0.95] × 2H₀`.

**`H` never moves during a solve.** The half-way bound's correctness rests on each path crossing
`H` exactly once, and a label discarded under one `H` cannot be recovered under another.  So a
moving `H` changes how the work is split, never the answer — and the equivalence suite checks that
across its whole sweep.

From **C++**, set `dynamic_half_way` and keep the algorithm object alive between solves; the
controller lives on it:

```cpp
AlgorithmParams<LabelList<Composition>> params;
params.direction = SearchDirection::Bidirectional;
params.critical_resource_index = 1;
params.half_way_point = 500.0;      // H0: where the controller starts, and R = 2 * H0
params.dynamic_half_way = true;

auto algorithm = graph.create_algorithm<SimpleDominanceAlgorithm>(params);
auto* bidirectional = as_bidirectional(algorithm.get());  // the controller lives behind it
for (/* each pricing iteration */) {
    // ... update reduced costs ...
    SolveResult result = graph.solve(algorithm.get(), -1e-9);
    // bidirectional->half_way_controller().h() is the H the next solve will use
}
bidirectional->half_way_controller().set_frozen(true);   // e.g. once the root node has converged
```

The one-shot `graph.solve<Algo>(params)` builds a fresh algorithm every call, so the controller is
discarded with it — which is also why `dynamic_half_way` is **not** a Python parameter.  From
**Python**, keep a `HalfWayController` yourself:

```python
from rcspp import HalfWayController

controller = HalfWayController(500.0)          # H0
for iteration in range(max_iterations):
    p.half_way_point = controller.h
    result = rg.solve(algorithm="bidirectional", params=p)
    controller.update(result)                  # returns the HalfWayMove it made
```

Pass `truncated=True` to `update` if you capped the search with `num_labels_to_extend_by_node`: a
bidirectional solve still reports `complete` then, so the status cannot say so.  The knobs — dead
zone, first step, decay, floor, clamp — are on `HalfWayControllerParams`, with RouteOpt's values as
defaults.

Two limits worth knowing.  `H₀` also fixes the range `R = 2H₀` the controller keeps `H` inside, so a
badly chosen `H₀` cannot be corrected past `1.9 H₀`; start from roughly half the clock's real
range, as for a static `H`.  And the signal is **surviving** labels, as RouteOpt's is: a forward
label stopped past `H` is still stored as a join boundary, so on a small graph the counts can stay
flat while `H` moves, and the controller then keeps stepping until the clamp.  That costs speed,
not correctness.

**It is not a default, and today it is not a speed-up.**  Measured with
`examples/cpp/bidirectional_cg_main.cpp` over whole column generations (Release, Gurobi 13.0,
`H₀ = R/2`), every instance proving the same LP bound as the forward pricer:

| Instance | Imbalance, static → dynamic | Dominance checks | Columns the join returned | Pricing time |
|---|---|---|---|---|
| R101_25 | 1.97× → 1.44× | −3 % | 189 → 190 | ≈0 s both |
| R201_25 | 2.83× → 2.04× | −28 % | 83 k → 170 k | 0.37 → 0.54 s |
| R202_25 | 1.69× → 1.70× | −14 % | 2.47 M → 2.64 M | 11.3 → 11.1 s |
| R201_50 | 2.34× → 1.44× | −6 % | 1.19 M → 1.84 M | 5.1 → 7.2 s |
| RC201_50 | 2.26× → 1.36× | −11 % | 0.70 M → 1.86 M | 4.5 → 8.4 s |

The controller does what it is for: the split is more balanced and dominance does less work.  But a
more central `H` lets more pairs cross it, the join returns *every* improving pair, and recording
them costs more than the labeling saves — time per column actually fell on every instance, while
the column count rose up to 2.7×.  Until the join's output is bounded, balancing the searches
mostly feeds the join.  Imbalance is the geometric mean over iterations of `max(f, b) / min(f, b)`.

`join_column_budget` bounds it: the join then keeps only its cheapest that many paths (see
"Parameters that behave differently here"), and the search stays exhaustive, so the solve still
ends `COMPLETE` and the controller keeps learning.  `stop_after_X_solutions` caps the join too, but
it also stops the *searches* once that many solutions exist, and a solve stopped that way ends
`MAX_SOLUTIONS`, which the controller does not learn from.  A join truncated by `max_join_pairs`
is not learned from either.

### One thing `status` cannot tell you

Every algorithm, not only this one, prunes its label queues when RSS crosses
`memory_pressure_fraction ×` the effective limit, and tightens the per-node extension quota
along with it.  Labels that were never extended are abandoned, so the answer may no longer be
optimal — but `status` still reads `complete`, because the label sets really were exhausted *of
what survived the trim*.  There is no status value for "exhausted but lossy", and reusing
`memory_limit` would conflate a hard stop with a soft trim, so it is reported as a flag:

```python
result = rg.solve(algorithm="bidirectional", params=p)
if result.memory_pressure_triggered:
    print("memory pressure trimmed this solve; the answer is not a proof")
```

Code that treats `complete` as proof of optimality — a column-generation loop deciding it has
converged, say — has to read this too.  `could_be_non_optimal()` will not tell you: it reads the
*parameters*, and memory pressure is a property of the run.

## Relaxing dominance for one solve

`dominance_ignored_components` leaves chosen components out of the dominance test, for one solve.
Two labels that differ only in those components are then compared as if they were equal there,
so labels are discarded that an exact search would keep. That makes it a heuristic: the solve may
miss the optimum, and `could_be_non_optimal()` says so. Every column it returns is still feasible
and correctly priced, since feasibility, the cost and the bidirectional join never read dominance.

```cpp
AlgorithmBaseParams params;
params.dominance_ignored_components = {2};   // leave the load out of dominance
auto relaxed = graph.solve<SimpleDominanceAlgorithm>(params);
```

Components are numbered as the model checks' messages number them: type slots in the order of the
`ResourceGraph`'s template parameters, then registration order within a type. With
`ResourceGraph<RealResource, IntResource>` holding a cost and a time window in the real slot and a
load in the int slot, the cost is 0, the time 1 and the load 2. A number the model does not have
throws `std::invalid_argument`.

It pays where the relaxed resource rarely decides feasibility. On 50-customer Solomon pricing
graphs with a loose capacity, leaving the load out kept the optimum on 19 of 20 instance and memory
combinations, at 16–87 times less time than the exact search; on C101, where the capacity binds, it
reached 88 % of the optimum. Leaving an ng memory out is weaker.

Every labeling algorithm honours it — `Simple`, `Pushing`, `Pulling`, `AStar` and `Bidirectional`,
the last in both of its searches. With a non-empty set, dominance is tested component by component
over the other components, as the default `CompositionDominanceFunction` does, whichever
composition dominance function the model was built with. With `LabelBuckets`, leaving out the
bucket or sort resource weakens the relaxation, since the buckets' early exits assume those
resources order dominance. The dives have no dominance and ignore it.

In a bidirectional solve, naming the clock turns the half-way bound off, as a clock outside the
dominance order does, and `half_way_off_reason()` says so. A relaxed solve does not teach the
dynamic half-way controller, whose label counts would not be the exact search's.

## `Greedy`

Extends labels greedily (best-cost-first) with limited backtracking.
Much faster than the exact algorithms; no optimality guarantee.

```python
params = AlgorithmParams()
params.num_labels_to_extend_by_node = 5   # extend at most 5 labels per node

result = rg.solve(algorithm="greedy", params=params)
```

## `Tabu`

Tabu-arc metaheuristic.  At each iteration it removes the arcs used in the
current solution from the graph and re-solves, diversifying the search.
Arc removal is temporary (governed by `tabu_tenure`).

```python
params = AlgorithmParams()
params.tabu_tenure        = 5
params.tabu_random_noise  = True      # random ±1 tenure noise
params.max_iterations     = 100
params.forbidden_tabu     = {source_id, sink_id}  # never remove arcs from these

result = rg.solve(algorithm="tabu", params=params)
```

## `Diversification`

Wraps another algorithm (default: `Greedy`) and repeatedly solves the
subproblem after removing solution arcs, collecting a pool of diverse
solutions.

```python
params = AlgorithmParams()
params.stop_after_X_solutions = 20    # collect 20 distinct solutions
params.max_iterations         = 100
params.seed                   = 42

result = rg.solve(algorithm="greedy", params=params)
# In C++, use DiversificationSearch<Comp> explicitly to wrap any inner algorithm
```

## Search directions

A labelling search runs **forward** by default: from the sources, along each node's out-arcs, until
its labels reach a sink. It can also run **backward**, from the sinks along in-arcs until its labels
reach a source, or **bidirectionally**, a forward and a backward half joined where they meet. The
direction is an argument of `solve` in Python, and a parameter, `AlgorithmBaseParams::direction`,
in C++:

```python
result = rg.solve(algorithm="simple", direction="backward")  # "forward" (default), "bidirectional"
```

```cpp
rcspp::AlgorithmBaseParams params;
params.direction = rcspp::SearchDirection::Backward;
const rcspp::SolveResult result = graph.solve<rcspp::SimpleDominanceAlgorithm>(params);
```

`algorithm="bidirectional"` is `simple` with `direction="bidirectional"`, and refuses any other
direction.

A backward or bidirectional search returns the same kind of paths as a forward one, in forward
order: each starts at a source and ends at a sink. A bidirectional search also reads
`critical_resource_index` and `half_way_point` ([`Bidirectional`](#bidirectional)); the class that
runs it is internal.

| Algorithm | Forward | Backward | Bidirectional |
|---|---|---|---|
| `Simple` | ✓ | ✓ | ✓ |
| `Pushing`, `Pulling`, `AStar` | ✓ | — | — |
| `Greedy`, `Tabu`, `Diversification` | ✓ | — | — |

An algorithm asked for a direction it does not support throws `std::invalid_argument` (a
`ValueError` in Python): when
`ResourceGraph::create_algorithm` builds it, or, for one you constructed yourself, when it solves
(`Algorithm::supported_directions()` lists what it supports). A backward search keeps its labels
in a `LabelList` that compares them backward; `LabelBuckets` has no backward form, so a backward
search with buckets is refused. Since the class that searches backward is not the one you name,
`create_algorithm<Strategy>(params)` returns a `std::unique_ptr<Algorithm<R, LC>>`.

**The model checks run first, always.** A backward or bidirectional solve runs the checks its
direction needs ([Model checks](#model-checks)) on the whole model before anything else: before any
preprocessing, whatever `preprocess` says, and also when `Algorithm::solve` is called directly, or by
an algorithm such as `Diversification` that runs another one inside it. So `preprocess` only says
whether the solve may reduce the graph; it never skips a check. A forward search needs no check.

## Model checks

A forward search reads nothing but the forward functions. A search that also extends labels
**backward**, from the sinks, reads each resource's backward semantics: its `backward_kind()`, its
backward extension, start and feasibility test. A **bidirectional** search also **joins** a forward
and a backward half into one path, so it reads each component's `join_rule()` as well. A model whose
backward semantics are missing or incoherent would let those searches return infeasible paths, or
none, while reporting `complete`. The model checks find that before any label exists:

| Check | Needed by | What it checks |
|---|---|---|
| `BackwardExtensionCheck` | backward and bidirectional searches | every component and the composition can extend, start, test and compare backward, and the backward values mean what the forward search enforces |
| `JoinCheck` | bidirectional searches | every component declares how two halves join, and the halves the join adds are sums |

`ResourceGraph::check_model(direction)` runs the checks a search in that direction needs, without
solving, and returns one line per problem (an empty `problems` list means the model is ready); in
Python, `rg.check_model("backward")` returns the list, for `"bidirectional"` by default:

```cpp
const rcspp::ModelReport report = graph.check_model(rcspp::SearchDirection::Bidirectional);
for (const auto& problem : report.problems) {
    std::cerr << problem << '\n';
}
```

The checks read the whole model, arcs that preprocessing removed included, so a verdict never
depends on what one solve's bound happened to remove. They never change the graph. A backward or
bidirectional solve runs them itself, before any preprocessing and whatever `preprocess` says, and
refuses a model that fails them with a `ModelRefused` (a
`std::runtime_error`, so Python sees a `RuntimeError`):

```text
a bidirectional search cannot run on this model:
  - component 1: arc 1 -> 2 consumes -3.000000, but its backward reading assumes no arc
    lowers the value: a backward label carries only a ceiling, so a path dipping below the
    floor would be accepted; loads must be non-negative
```

There are twenty complaints, in two groups. The last column says which searches each one refuses:
a backward search is refused for fourteen of them, a bidirectional search for all twenty.

**Something was not declared, or was declared for the wrong kind of resource:**

| Complaint | What to do | Refuses |
|---|---|---|
| `no node carries a resource` | The checks read the declarations off a node's resource; give the nodes their resources. | both |
| `its extension function declares no backward_kind()` | Derive the extension function from `BackwardForm` or a form built on it (`resource/functions/extension/backward_form.hpp` explains them), or declare `static constexpr BackwardKind kind`. | both |
| `its feasibility function declares no join_rule()` | Return a `JoinRule` from `join_rule()` — `AlwaysTrue` if the resource genuinely cannot block a join, and say why. | bidirectional |
| `its feasibility function declares JoinRule::ValueOrder … on a resource that has none` | `ValueOrder` is `forward <= backward` on two scalar values.  A container resource has no such order: declare `JoinRule::Custom` and write the test. | bidirectional |
| `the model's composition dominance function has no backward form` | A composition dominance function you passed to `ResourceGraph`'s constructor overrides `check_dominance` only, which is all a forward solve needs.  Override `check_back_dominance` too, or keep the default `CompositionDominanceFunction`. | both |
| `the model's composition extension function has no backward form`, `… has no backward start`, `… feasibility function has no backward form` | The same for a composition extension or feasibility function: one that overrides only `extend` or `is_feasible` compiles and solves forward, but has no `extend_back`, `start_back` or `is_back_feasible`.  Override those too, or keep the defaults `CompositionExtensionFunction` and `CompositionFeasibilityFunction`. | both |
| `the model's composition feasibility function has no join test` | The same for `can_be_joined`.  A `can_be_joined` of your own must be exact, like a component's: no check can tell one that refuses too much. | bidirectional |
| `the model's cost function is not additive` | A backward label that reaches a source is a complete path, and the join prices a path as the forward half's cost plus the backward half's.  That is exact only when every component the cost reads has a zero cost (`TrivialCostFunction`) or a value cost (`ValueCostFunction`) under an accumulating extension.  A threshold's backward value is a deadline or a remaining capacity, not a cost, so a `CompositionCostFunction` over a time window with a `ValueCostFunction` is refused.  Give that component a `TrivialCostFunction`, or keep the default cost function, which reads component 0 only. | both |

The `join_rule()` one has **two causes, and only one of them is yours.** A function you wrote that
never answered is the first. The second is a library function that answers `Unspecified` on
purpose, because the configuration you gave it has no backward reading: its test is a question
about what a label has collected *so far* — a predicate on a **prefix** — and a backward label
carries a **suffix**. Each of these would fail silently, returning an infeasible path or nothing at
all with a `complete` status, so they are refused instead:

| Component | When it refuses | What to do instead |
|---|---|---|
| `IntersectionFeasibilityFunction` | **required** values (`forbidden = false`) | Model the requirement on the whole path, not per node: there is no backward form of "collected one of these already". |
| `IntersectionFeasibilityFunction` | **forbidden** values that are not `{v}` at node `v` | Use the ng-route condition, `forbidden_by_node[v] = {v}`, with `NgPathExtensionFunction`: see "One rule the join imposes" under [`Bidirectional`](#bidirectional). |
| `SizeFeasibilityFunction` | a non-zero **minimum** size anywhere, or a **per-node cap** that differs from the default | Use one cap for every node, or solve forward.  A single cap is suffix-safe: a backward label that reaches a source holds the whole set, and every set along the path is a subset of it.  A floor or a per-node cap bounds what the path has collected *up to* a node, which a suffix cannot see. |
| `MinMaxFeasibilityFunction` | a non-zero **minimum** anywhere, default or per node, under a threshold extension (`CapacityExtensionFunction`, `TimeWindowExtensionFunction`) | The same fix, for the same reason: a backward label carries only a *ceiling*, so neither the join nor a backward label that reaches a source can tell whether the forward value got as high as the floor. |
| `MinMaxFeasibilityFunction` | under an accumulating extension (`AdditionExtensionFunction`), any window other than a single `[0, max]` | Use `CapacityExtensionFunction`.  A per-node window is the same prefix question as a per-node size cap, and a non-zero minimum rejects the empty suffix a backward label starts from; either way the sum of the two halves cannot be tested exactly. |
| `ReachableFeasibilityFunction` | always | Its `is_reachable` asks what the label has already collected, which a suffix cannot answer. |

In every one of these, forward-only solves are unaffected.

**Or two declarations were each legal on their own and incoherent together.** All ten of these
are a *pairing* fault: nothing is wrong with either half in isolation, which is why only a check
that sees both catches them.

The clamps a threshold extension applies are not declared: the checks observe them, by running the
extension on values beyond every bound (the infinities, or an integral type's extremes) through an
arc leaving and an arc entering each node, so a threshold of your own is checked however it is
written, and must accept any value of its type.  The same goes for where a backward label starts at
a sink: the checks call `start_back` through an arc entering the sink, as a backward search seeds
it.  Each clamp and start is then put to the node's forward test, which must admit it and reject a
value just above it: a backward label starts at, and is clamped to, the largest value the forward
test admits.  So that test must handle any value, and be exact at its ceiling.  A feasibility
function's floor is not declared either: the checks ask the node's backward test directly, at the
lowest value a forward label can hold there (the forward clamp, or 0), so that test must handle any
value too.  An arc that preprocessing removed still counts, here and in the negative-load scan
below.  A node no arc leaves is never the arrival of a backward step, so its backward clamp is not
checked, though a sink's start is; likewise the forward clamp at a node no arc enters.

| Complaint | What it means | Refuses |
|---|---|---|
| `its extension accumulates but its feasibility function declares JoinRule::ValueOrder` | `ValueOrder` compares the forward value against the backward one, which reads the backward value as a *bound*.  Under an accumulation it is not a bound, it is the suffix's own consumption, so the comparison tests nothing the model contains — and it fails by *accepting*.  Use a threshold extension, or declare `JoinRule::Custom` with a body that adds the two halves. | bidirectional |
| `its feasibility function rejects a backward value of … at node …, the lowest value a forward label can hold there` | The feasibility function's backward test has a floor, a node's opening time say, above that value: the extension never waits for it going forward, and the forward test does not check it either.  The backward search would then reject deadlines a forward path meets.  Build the extension and the feasibility function from the same windows (`TimeWindowExtensionFunction` with `TimeWindowFeasibilityFunction`), or pair `CapacityExtensionFunction` with `MinMaxFeasibilityFunction(0, capacity)`. | both |
| `its backward labels at node … are not clamped at all, but its feasibility function rejects values above some bound there` | The extension's backward step does not clamp, so a deadline can stay above the node's upper bound and the backward search admits deadlines the forward search rejects — under a time window's backward test, which reads only the opening time, the join or a backward label reaching a source would accept an infeasible path.  Clamp each backward label to its node's upper bound, as `ThresholdForm` does, built from the feasibility function's `NodeBounds`. | both |
| `its backward labels at sink … start at the type default, but its feasibility function rejects values above some bound there` | The extension's `start_back` sets nothing, so a backward label at the sink keeps the type default, 0, as its deadline, and the backward search rejects deadlines a forward path meets.  Start each backward label at the sink's upper bound, as `ThresholdForm::start_back` does. | both |
| `its backward labels at node … are clamped to …` (or `at sink … start at …`) `, which its feasibility function rejects there` | A threshold extension clamps each backward label to its own upper bound at the node.  If the node's `is_back_feasible` rejects that value, every backward label there is lost: the extension was built with other caps than the feasibility function, typically `CapacityExtensionFunction(capacity)` beside per-node caps on the feasibility function.  Build both from one `NodeBounds` (`make_node_bounds(0, capacity, per_node)`, or `feasibility->bounds()`), or use `presets::add_capacity_resource`. | both |
| `its backward labels at node … are clamped to …` (or `at sink … start at …`) `, above` (or `below`) `the largest value its feasibility function admits there` | The same mismatch where the node's backward test cannot see it.  The node's ceiling is the largest value its forward test admits.  A clamp or start *above* it lets the backward search admit deadlines the forward search rejects — a time window's backward test reads only the opening time, so the join would accept infeasible paths; one *below* it rejects deadlines a forward path meets.  The fix is the one above.  A forward test of your own that admits values a little above its ceiling, beyond the library's own `epsilon` tolerance, reads as a ceiling higher than the clamp and is refused: make it exact. | both |
| `arc … consumes …, but its backward reading assumes no arc lowers the value` | A backward label carries only a ceiling, so it cannot see a path's value dip below a floor inside the suffix: with a negative load, a `MinMaxFeasibilityFunction(0, capacity)` under `CapacityExtensionFunction` or `AdditionExtensionFunction` accepts a path whose load went below 0.  Loads must be non-negative.  A time window is exempt, since it waits at each node's opening time. | both |
| `its extension declares BackwardKind::Accumulate, but along … it is not a sum` | The join adds the component's two halves, through its cost (`ValueCostFunction`) or its feasibility function's join test (`MinMaxFeasibilityFunction` under an accumulation tests `forward + backward`), which is exact only if a path's value is the sum of one amount per arc.  `Accumulate` promises less: that the step is the same in both directions.  A bottleneck, the largest load seen, is such a step, but its halves 3 and 5 add up to 8 where the path's value is 5.  The check runs the extension: from the type default, two steps along the model's arcs must give the sum of the two single steps.  Write the step as an addition, or keep the component's halves apart: a `TrivialCostFunction`, and a feasibility function whose join test does not add. | bidirectional |
| `its extension declares BackwardKind::Accumulate, but starts its backward labels at sink …` | A forward label starts at the type default, so a backward label whose halves are added must start there too: any other start is counted on top of the path's own sum.  Leave `start_back` to its default. | bidirectional |
| `its feasibility function forbids each node at itself … does not declare BackwardKind::ArcEndpoints` | The feasibility test asks *am I already in my own memory*, which needs the memory at a node to exclude that node in **both** directions.  Only an `ArcEndpoints` extension gives that.  A `BackwardKind::ArcValue` container takes its elements from the arc's *value*, which is the same object going each way, so the backward label reaches a node already holding it and is rejected there — every time.  Use `NgPathExtensionFunction`, or `presets::add_elementary_resource` for an elementary path. | both |

(The messages are generated by `BackwardExtensionCheck` and `JoinCheck`, in
`cpp/rcspp/validation/`; if you change a message there, change these tables too.)

A **capacity written as an addition**, `AdditionExtensionFunction` with
`MinMaxFeasibilityFunction(0, capacity)`, is not one of them: `MinMaxFeasibilityFunction` picks its
join test from the extension it is paired with.  Under an addition a backward label starts at the
empty suffix and counts its load *up* from zero, and the join tests `prefix + suffix <= capacity`.
That is exact while loads are non-negative, which the checks verify.  `CapacityExtensionFunction`
is the other way to write it: additive forward, a threshold backward.

One pairing needs no refusal: a time window's extension with a feasibility function whose floor is
lower, such as `MinMaxFeasibilityFunction(0, cap)`.  A threshold extension marks a backward deadline
below the node's opening time as unmeetable itself, whatever the feasibility function tests.

A threshold extension clamps its backward label to each node's upper bound **as its own bounds state
it**, so those must be the feasibility function's.  Give both functions one `NodeBounds`:

```cpp
auto caps = rcspp::make_node_bounds(0.0, capacity, per_node_caps);  // {node: {0, cap}}
graph.add_resource<RealResource>(
    std::make_unique<CapacityExtensionFunction<RealResource>>(caps),
    std::make_unique<MinMaxFeasibilityFunction<RealResource>>(caps),
    std::make_unique<TrivialCostFunction<RealResource>>(),
    std::make_unique<ValueDominanceFunction<RealResource>>());
```

`TimeWindowExtensionFunction` and `TimeWindowFeasibilityFunction` take a `NodeBounds` of windows the
same way, and every feasibility function that has one returns it from `bounds()`.  The checks refuse
a clamp that differs from the node's bound, in either direction.

## Bucket label containers

For graphs with many labels per node, `LabelBuckets` speeds up dominance
checking by partitioning labels on one resource dimension.

```python
from rcspp.graph import BucketAlgorithmParams

bp = BucketAlgorithmParams()
bp.range_buckets         = 200    # partition the time dimension into 200 buckets
bp.bucket_resource_index = 0      # resource 0 is time
bp.sort_resource_index   = 1      # sort within bucket by resource 1

result = rg.solve(algorithm="simple", params=bp)
```

In C++:

```cpp
BucketAlgorithmParams<LabelBuckets<Comp>> bp;
bp.range_buckets = 200;
bp.bucket_resource_index = 0;
bp.sort_resource_index = 1;
auto result = graph.solve<SimpleDominanceAlgorithm, RealResource, LabelBuckets>(ub, bp);
```
