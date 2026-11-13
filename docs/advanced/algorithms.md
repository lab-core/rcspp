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
| `Simple`, `AStar`, `Pushing` or `Pulling` with `direction="bidirectional"` | ✓ | fast–slow | Long routes with a tight, monotone clock |
| `AStar` | ✓ | fast–slow | Good heuristic available |
| `Greedy` | ✗ | fast | Quick feasible solution; large graphs |
| `BoundGuidedGreedy` | ✗ | fast | Greedy guided by a lower bound on the cost still to pay; with `prune_by_threshold` and a tight `upper_bound` |
| `Tabu` | ✗ | medium | Metaheuristic diversity |
| `ImprovingTabu` (C++) | ✗ | medium | Tabu that counts the episodes that do not improve |
| `Diversification` (C++) | ✗ | configurable | Generate a diverse pool of solutions |

✗: no guarantee within a budget (`max_iterations`). With no budget, a heuristic tries every path,
and so finds the optimum, at a cost that grows with the number of paths.

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

## Is the result a proof? `status` and `proven`

`status` says why a search stopped; `complete` means it ran out of labels to extend. A search can
finish and still not have proved anything:

- **Memory pressure.** Every algorithm prunes its label queues when RSS crosses
  `memory_pressure_fraction ×` the effective limit, and tightens the per-node extension quota
  along with it. The trim stores aside, for the next phase, the labels it takes off the queues (a
  side of a bidirectional search abandons them). A later trim releases what an earlier one stored
  aside, so those labels are never extended. `status` can then still read `complete`, because the
  label sets really were exhausted *of what survived the trims*. `memory_pressure_triggered`
  reports it.
- **An arc mask** that is not all open (C++, "Leaving arcs out of a solve" below; a soft-only mask
  too): the search was exhaustive on the graph without the masked arcs, not on the graph.
- **An admission rule that is not exact**: `Relaxed` or `KBest` ("Keeping fewer labels" below),
  or a strategy of one's own whose `exact` answers `false`.
- **A heuristic driver**: the tabu searches make the arcs of each path tabu between episodes. A
  solve that did so is not a proof. A tabu solve is a proof only when its first episode, with no
  arc tabu, found no path.
- **The incumbent prune on a cost that is not additive** (`prune_by_incumbent`): with no valid
  completion bound, it compares a label's cost alone, which may drop the cheapest path.

`result.proven` combines them: it is true when `status` is `complete` and none of these happened,
so that no path better than those returned exists (none at all, when none is returned).
`result.proof_lost_by` says why not: the status's name when the search did not finish
(`"truncated"`, `"timeout"`, ...), otherwise the first loss: `"memory_pressure"`,
`"arcs_masked"`, `"relaxed_admission"`, `"heuristic_driver"` or `"incumbent_prune"`. A quota that
cut nothing leaves the proof intact.

```python
result = rg.solve(params=p)
if not result.proven:
    print("not a proof:", result.proof_lost_by)
```

Code that treats `complete` as proof of optimality — a column-generation loop deciding it has
converged, say — should read `proven` instead. `could_be_non_optimal()` will not tell you: it
reads the *parameters*, and memory pressure is a property of the run. In C++,
`Algorithm::is_optimal()` answers `proven` for the last solve.

## `Simple` (default)

Labels are taken first in, first out (`parts::FifoSchedule`). Each label taken is extended along
every arc of its direction. A new label is kept at its node unless a label there dominates it, and
it evicts the labels it dominates. A label dominated while it waits is dropped when it is taken.
This is a label-correcting search: a label extended may still be dominated later. It searches
forward, backward and bidirectionally.

```python
result = rg.solve()                        # algorithm="simple" by default
result = rg.solve(algorithm="simple")
```

## `Pulling`

Like `Pushing`, node by node in sweep order (ascending positions forward, descending backward).
But each visit *pulls* into the node the labels of its predecessors (forward) or successors
(backward), instead of each label being *pushed* along its out-arcs. It searches forward, backward
and bidirectionally; bidirectionally, a label past the half-way point is stored but never pulled
from.

(bidirectional)=

## Bidirectional search (`direction="bidirectional"`)

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

result = rg.solve(direction="bidirectional", params=p)
```

`half_way_point` has no useful default: **0 turns the bound off**, it does not derive one.  The
algorithm has no way to read a feasibility function's upper bound, so `H` is the only number it
has to go on — it takes the range to be `[0, 2H]` rather than the other way round.  With the bound
off both searches run to completion and the join considers every pair, which is correct but
*slower* than a forward solve rather than faster, since it is a forward search plus a backward
search plus a join.  That is the supported way to ask for an unbounded run, and
`result.bidirectional.half_way_used` tells you which of the two you got.  A solve that wants
the speed-up sets `H` explicitly, to roughly half the clock's range.

Because the default gets you the slow case without asking for it, a solve that runs with the
bound off — for this reason or any of those in the next section — logs a warning saying why.  It
does so once per reason per process, since the Python bindings build a new algorithm for every
solve and a pricing loop would otherwise repeat it on every iteration; later solves log the same
sentence at debug level.  `result.bidirectional.half_way_off_reason` holds it for every solve,
empty when the bound was in force.

### The critical resource

One resource acts as the **clock** that says where "half-way" is.  It must be:

- **monotone** — never decreasing along an arc, so its value crosses `H` exactly
  once along any path;
- **bounded**, with a known finite range;
- **a threshold backwards** — read backward, it must carry a *ceiling* on the
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

If the chosen resource fails any of these, **the half-way point is switched off**
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
using Resources = rcspp::ResourceTypeComposition<rcspp::RealResource, rcspp::IntResource>;
using Labels = rcspp::LabelList<Resources>;
rcspp::AlgorithmParams<Labels> params;
params.direction = rcspp::SearchDirection::Bidirectional;
params.critical_resource_index = 0;  // the first int component
params.half_way_point = 500;
// cost type RealResource, clock type IntResource
auto result = graph.solve<rcspp::strategy::Simple, Labels, rcspp::RealResource,
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

[Model checks](#model-checks) lists the twenty-two complaints, what each means and what to do, and
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

<!-- docs-check: given double capacity = 10.0;
     std::map<size_t, std::pair<double, double>> per_node_caps; -->

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
`rcspp::declares_directional_resource_kind_v<ExtensionFunctionType>` (in
`rcspp/resource/functions/extension/extension_function.hpp`): the extension function declares a
`DirectionalResourceKind` other than `Unspecified`.

Every preset asserts it on its own extension, so a preset whose extension has no backward
reading does not compile.  It says nothing about the feasibility function paired with it: that
depends on the function's settings, which the model checks test before the solve.  For an
extension of your own, assert it yourself:

<!-- docs-check: given using MyExtension = AdditionExtensionFunction<RealResource>; -->

```cpp
static_assert(rcspp::declares_directional_resource_kind_v<MyExtension>);
```

`add_resource` itself does **not** assert it, on purpose.  A model that is never solved
bidirectionally owes the check nothing — a custom extension function with no declared kind
and an unsigned time window are both valid forward-only models — so a hard error there would
reject correct code.

So the trait is an **early warning on a subset**, not a replacement: every backward or
bidirectional solve runs the model checks first, from C++ and from Python alike, with the same
messages and component numbering.

The full inventory — all six checks, which fires where, and why no one of them subsumes
another — is in `cpp/rcspp/resource/functions/directional_resource_kind.hpp`.

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
  node being left" per direction — `DirectionalResourceKind::ArcEndpoints`. The memory at `v` is the
  nodes before it going forward and the nodes after it going backward. Correct.
- `UnionExtensionFunction`, `IntersectionExtensionFunction` and `DifferenceExtensionFunction`
  accumulate the arc's *value*, which is the same object both ways —
  `DirectionalResourceKind::ArcValue`. With the usual `{origin}` payload the backward memory at `v`
  contains `v` itself, so the feasibility test rejects every backward label the moment it is created
  and the backward search keeps nothing but its start label.

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
optimum than `simple` on the same graph, and a worse one with the half-way point on than with it
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
| `max_solutions` | stops the search once that many solutions exist | stops the *search* the same way, but never cuts the join short: the join still considers every pair, splices only the cheapest `max_solutions` distinct joined paths, and the result list is resized afterwards, so a `complete` status still means the search was exhaustive |
| `prune_by_incumbent` | drops a label whose own cost is at or above the incumbent — valid for a complete path, not for a frontier | drops a half whose cost *plus a lower bound on its completion* is at or above the incumbent, and drops a join that is no better than the incumbent. When the reduced-cost graph has a negative cycle there is no finite lower bound, and no half is dropped for its cost. With it off **and a finite `upper_bound`**, every join that bound admits is returned — which is what a pricing pool wants. With no `upper_bound` at all the join keeps pruning against the incumbent whatever the flag says: see below |
| `timeout_s`, the memory limit | stop the search; whatever was found is returned | stop the search, then **still run the join**, with the incumbent cutoff forced on. With the half-way point in force few forward labels reach a sink, so without the join a truncated solve would return almost nothing, and a pricing loop would stall on it. The join hands back the improving paths the two halves already hold — both halves, since the two searches are interleaved by the work each has done, so a stop finds each partly built; the status still says the run was cut short. `join_after_early_stop = false` skips the join instead |
| `should_stop` | stops the search | stops the search **and skips the join**: an interrupt means stop now |

Why the last row depends on `upper_bound` being finite: a solve that supplies none has expressed no
filter, and returning every admissible pair there is nobody's question.  Measured on the full C201
instance, doing so produced **3 065 288 solutions in 12.2 s** where the forward search finished the
same optimum in 0.68 s — while the bidirectional *search* extended 1.46× **fewer** labels.  On
RC201 the same run reached 9.3 GB resident.  None of that is search cost; it is recording and
de-duplicating the set.  So the incumbent cutoff stays on when there is no bound, and the flag is
honoured where it means something.

The bidirectional solution *set* is not the forward search's, and with a finite bound and pruning
off it is usually **larger**.  The join pairs every forward half with every backward half the
bound admits, and a pair that would be dominated at the sink is still returned: two halves are
each non-dominated where they meet, not as a whole path.  On Solomon pricing graphs at half the
horizon, with a uniform dual and pruning off, it returned 12 to 19 times the forward search's
columns.  Every one of them is feasible at its stated cost, and the optimum is the same.  If the
master should see only non-dominated columns, use a forward algorithm, or cap the count with
`max_solutions`, which the join honours without building the rest.

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
it misses relative to forward is the **half-way point**, not the join — the bound stops forward
labels before they reach a sink.

One caveat, and it only applies in modes that are already inexact.  When the per-node extension
quota binds — `max_extended_labels_per_node` (truncated labeling), or `on_memory_pressure` tightening
it automatically — a forward label can be stored and never grown, so it never produces the boundary
label the join reads, and that pair is lost.  At the default quota of "unlimited" this never arises.

### Quotas in a bidirectional search

A bidirectional search runs one phase. So when the per-node quota or memory pressure cuts a label
off a half's queue, that label is **abandoned**: it stays in its node's container, where the join can
still pair it, but it is never extended, and no later phase gives it back. That holds for every
algorithm's order. Such a solve reports `Truncated`, not `Complete`, and `is_optimal()` answers
`false`. `Pushing` and `Pulling` apply the quota at each visit of a node, as they do forward, so
under the same quota they extend more labels than `Simple` or `AStar`, which count a node's labels
over the whole search.

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

<!-- docs-check: given using TimeResource = RealResource; using DemandResource = IntResource;
     ResourceGraph<RealResource, IntResource>* graph = nullptr;
     std::map<size_t, std::pair<double, double>> windows; int capacity = 10; -->

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
  result = rg.solve(direction="bidirectional", params=p)
  stats = result.bidirectional  # None from every other algorithm
  if not stats.half_way_used:
      print("the clock was rejected; this solve was slower than a forward one")
  print(stats.number_of_joined_paths, "paths came from the join")
  ```

  `half_way_used` is worth checking rather than assuming. `number_of_joined_paths`
  tells the two payoff failures apart: zero with a correct answer means the answer came
  from a search reaching a terminal rather than from the join, so `H` is placed such that
  nothing crosses it.

## `Greedy`

A dive: from each label it takes, it extends along every arc and goes on with the cheapest child,
keeping the other children for later; at a dead end it goes back to the last label still waiting.
Each path to a sink is recorded when its label is taken. It compares no labels: every feasible label
is kept until it is extended, and none is stored at the nodes. A label is released once its
children are made, and lives on only while a label extended from it still waits. With no budget it
tries every path, and so finds the optimum, but its work grows
with the number of paths; with one, it is fast, with no optimality guarantee.

Like the tabu searches, it dives forward from the sources by default, or backward from the sinks
(`direction="backward"`), with the model's backward semantics; neither searches bidirectionally.
Backward and forward dives under the same budget find different columns: on fixed duals, backward
Tabu and Improving Tabu found cheaper best columns than forward ones on four of five R2 and RC2
pricing graphs, the families where forward dives do worst, and dearer ones on others, so a
pricing loop can run both.

As in every labelling search, `max_iterations` counts the labels taken, and
`max_extended_labels_per_node` (with `max_phases`) caps the labels taken at each node.

```python
params = AlgorithmParams()
params.max_iterations = 10_000            # take at most 10 000 labels
params.max_extended_labels_per_node = 5   # extend at most 5 labels per node

result = rg.solve(algorithm="greedy", params=params)
```

### Bound-guided Greedy

`BoundGuidedGreedy` (`algorithm="bound_guided_greedy"`) is Greedy's dive with a label's children
taken by cost plus a lower bound on the cost still to pay, not by cost alone, so each step goes
toward the cheapest completion. It is meant with `prune_by_threshold`, which drops, under the same bound, the
labels that cannot beat the upper bound. The bound costs one Bellman–Ford run at the start of each
solve. With no budget, it finds the optimum, as Greedy does.

```python
params = AlgorithmParams()
params.max_iterations = 20_000
params.prune_by_threshold = True
result = rg.solve(algorithm="bound_guided_greedy", upper_bound=0.0, params=params)
```

On fixed duals, with 20 000 labels and an upper bound of 0, its best column was never dearer than
that of plain Greedy without the prune on 25 pricing graphs, and cheaper on four: C101 at 25 and 50
customers (-401.5 to -524.1, -429.7 to -579.3) and R101 at the first two column-generation
iterations measured (-245.7 to -319.9, -16.0 to -23.7). The order accounts for the cheaper
columns: with the prune on both, the same four are cheaper (R101's second: -19.8 to -23.7). On the
others, it found the same best column in about the same time.

## `Tabu`

Episodes of Greedy's dive. Each episode dives from the sources and ends at the first path it
reaches; the arcs of that path then become **tabu** for the next `tabu_tenure` − 1 episodes (plus
an adaptive extra, which grows after an episode that found nothing new and shrinks after one that
did): the episode that makes an arc tabu also ages the list, so a tenure of 1 makes no arc tabu. A
dive releases a child reached along a tabu arc, unless every child is (aspiration by default): it
can always go on. The graph is
not edited: the tabu arcs are the soft arcs of the search's mask. `max_iterations` counts
episodes, and must be finite. An episode that runs out of paths while no arc is tabu has proved
that no path exists, and ends the solve; any other result is not a proof (`result.proven` is
false).

```python
params = AlgorithmParams()
params.tabu_tenure        = 5
params.tabu_random_noise  = True      # random ±1 tenure noise
params.max_iterations     = 100
params.tabu_exempt_node_ids     = {source_id, sink_id}  # never make their arcs tabu

result = rg.solve(algorithm="tabu", params=params)
```

`ImprovingTabu` (C++, `strategy::ImprovingTabu`) dives once with no arc tabu, then runs
`max_iterations` tabu episodes, counting those that find no path strictly cheaper than the best
so far: `non_improving_episodes_before_growth` of them in a row grow the tenure. Each episode
still stops at its first path, whatever it costs; only the count reads the best cost. With
`max_iterations = 0`, ImprovingTabu returns its construction; Tabu, which has no construction,
returns nothing.

## `Diversification`

Wraps another algorithm (default: `Greedy`) and solves the graph again and again, each time
leaving out the arcs of the paths found so far for `tabu_tenure` iterations, collecting a pool of
diverse solutions. The arcs are left out through the inner algorithm's arc mask: the graph is not
copied or edited, so arc ids, and the order in which a node's arcs are tried, stay the graph's.
`max_iterations` counts the inner solves, and must be finite.

Diversification is C++ only: Python does not offer it.

<!-- docs-check: skip -->

```cpp
AlgorithmParams<LabelList<Resources>> params;
params.max_solutions = 20;    // collect 20 distinct solutions
params.max_iterations = 100;  // at most 100 inner solves
params.tabu_tenure = 5;       // a path's arcs stay out for 5 solves
params.seed = 42;
// Greedy by default; strategy::Diversification<strategy::AStar> diversifies another
const SolveResult result = graph.solve<strategy::Diversification<>>(ub, params);
```

## Leaving arcs out of a solve

An `ArcMask` leaves arcs out of a solve without editing the graph. A **hard** arc is never used: no
labelling search extends a label along it, in any direction, as if it were not in the graph. A
**soft** arc is used as any other, unless the search's schedule reads it (a depth-first schedule's
child filter may). Arc ids, and the bounds a search computes on the whole graph, are unchanged, so every path
returned names the graph's own arcs, and A\*'s bound is still a lower bound.

```cpp
rcspp::ArcMask mask(graph.get_arc_id_bound());   // every arc open
mask.set(0, rcspp::ArcMask::State::Hard);        // leave arc 0 out

auto algorithm = graph.create_algorithm<strategy::AStar>();
algorithm->set_arc_mask(&mask);  // read at the start of each solve; it must outlive them
const SolveResult result = graph.solve(algorithm.get(), ub);
```

The mask is sized by `get_arc_id_bound()`, one past the largest arc id, removed arcs included; an
id it was not sized for is open. It is a C++ feature: Python does not offer it.

## Dropping labels that cannot beat the upper bound

`prune_by_threshold` has a labelling search drop a label when no path through it could be
returned: when its cost plus a lower bound on the cost still to pay from its node is at or above
the `upper_bound` the solve was given. The bound is computed at the start of each solve, by one
Bellman–Ford run over the arcs' shares of the cost, from the end nodes. The paths returned are
those of the solve without it, and fewer labels are extended. It pays when the upper bound is
tight: in pricing, `upper_bound=0` asks only for columns of negative reduced cost.

```python
params = AlgorithmParams()
params.prune_by_threshold = True
result = rg.solve(upper_bound=0.0, params=params)   # the same columns, fewer labels
```

With no upper bound, only labels that reach no end node are dropped. A negative-cost cycle leaves
no finite bound, and nothing is dropped. A margin of `tolerance` (relative to the upper bound when
that is large) keeps rounding in the bound from dropping a path that would be returned. The bound
needs an additive cost: every resource the cost reads has a zero cost (`TrivialCostFunction`) or a
value cost (`ValueCostFunction`) under an addition. Another cost raises `ValueError`
(`std::invalid_argument`) when the solve starts.

Every labelling search reads it, Greedy and the tabu searches included, in every direction, and
both halves of a bidirectional search, each against its own end nodes. Unlike `prune_by_incumbent`,
it compares with the fixed upper bound, not the best path found so far, so it never cuts a path
the solve would return.

## Keeping fewer labels: relaxed and k-best admission

A labelling search keeps a new label at its node unless a label there dominates it.
`admission_rule` makes it keep fewer, for one solve:

- **`Relaxed`** leaves the components in `dominance_ignored_components` out of the dominance test.
  Two labels that differ only in those components compare as if they were equal there, so labels
  are dropped that an exact search keeps.
- **`KBest`** keeps, after dominance, at most `max_admitted_labels_per_node` labels at each node, the cheapest: at a
  full node, a new label takes the place of the costliest when it costs less, and is dropped
  otherwise.

Either is a heuristic. Every path returned is feasible and correctly priced, since feasibility and
the cost never read dominance, but the optimum may be missed: `result.proven` is false, and
`proof_lost_by` reads `"relaxed_admission"`. At its limit a rule is exact, and the proof stands: a
relaxed rule that leaves nothing out, a k-best rule with no limit.

```python
from rcspp.graph import AdmissionRule

params = AlgorithmParams()
params.admission_rule = AdmissionRule.Relaxed
params.dominance_ignored_components = {2}   # the third resource registered
relaxed = rg.solve(params=params)

params = AlgorithmParams()
params.admission_rule = AdmissionRule.KBest
params.max_admitted_labels_per_node = 5                      # at most 5 labels at each node
k_best = rg.solve(params=params)
```

In Python, a resource is named by its position in the order the resources were registered, the
cost's being 0. In C++, components are numbered as the model checks' messages number them: type
slots in the order of the `ResourceGraph`'s template parameters, then registration order within a
type. With `ResourceGraph<RealResource, IntResource>` holding a cost and a time window in the real
slot and a load in the int slot, the cost is 0, the time 1 and the load 2, whatever the order they
were registered in:

```cpp
AlgorithmBaseParams params;
params.admission_rule = parts::AdmissionRule::Relaxed;
params.dominance_ignored_components = {2};  // leave the load out of dominance
const SolveResult relaxed = graph.solve<strategy::Simple>(params);
```

A component the model does not have, or an `max_admitted_labels_per_node` of 0, raises `ValueError`
(`std::invalid_argument`) when the solve starts; a field the chosen rule does not read is warned
about.

The relaxed rule is meant for a resource that rarely decides feasibility, a loose capacity say:
leaving out one that binds drops the labels whose paths the optimum needs. The relaxed test
compares component by component over the others, as the default `CompositionDominanceFunction`
does, whichever composition dominance function the model was built with. With bucket
containers, leaving out the bucket or sort resource weakens the relaxation, since the buckets'
early exits assume those resources order dominance.

Every labelling search reads the rule (`simple`, `pushing`, `pulling`, `astar`), in every
direction, and so do both halves of a bidirectional search; Greedy and the tabu searches keep
every label and ignore it. In a bidirectional solve, leaving the clock out of dominance turns the
half-way point off, as a clock outside the dominance order does, and
`result.bidirectional.half_way_off_reason` says so. The k-best rule keeps the half-way point: a
label past `H` may then take the place of one below it, which the search would have extended.

## Search directions

A labelling search runs **forward** by default: from the sources, along each node's out-arcs, until
its labels reach a sink. It can also run **backward**, from the sinks along in-arcs until its labels
reach a source, or **bidirectionally**, a forward and a backward half joined where they meet. In
C++, the direction is a parameter, `AlgorithmBaseParams::direction`:

```cpp
rcspp::AlgorithmBaseParams params;
params.direction = rcspp::SearchDirection::Backward;
const rcspp::SolveResult result = graph.solve<rcspp::strategy::Simple>(params);
```

In Python, it is an argument of `solve`, and a direction the algorithm does not search raises
`ValueError`:

```python
result = rg.solve(algorithm="simple", direction="backward")  # "forward" (default), "bidirectional"
```

With `direction="bidirectional"`, `simple`, `astar`, `pushing` and `pulling` each run their own
order on both halves.

Whatever its direction, a search returns the same kind of paths, in forward order: each starts at a
source and ends at a sink.
A bidirectional search also reads `critical_resource_index` and `half_way_point`
([Bidirectional search](#bidirectional-search-directionbidirectional)); the class that runs it is internal.

| Direction | Searched by |
|---|---|
| Forward | every algorithm |
| Backward | `Simple`, `AStar`, `Pushing`, `Pulling`, and the heuristics: `Greedy`, `BoundGuidedGreedy`, `Tabu`, `ImprovingTabu` |
| Bidirectional | `Simple`, `AStar`, `Pushing`, `Pulling` |

Backward, each labelling algorithm runs as it does forward, on the graph read in reverse. `Pushing`
and `Pulling` sweep the nodes in descending position order, and `Pulling` pulls into a node from
its successors. `AStar` bounds the cost still to pay from a source to a node, with a Bellman-Ford
run from the sources.

Bidirectionally, each runs its own order on both halves, with the same half-way point and the same
join. `AStar` takes each half's labels lowest cost plus that half's completion bound first.
`Pushing` and `Pulling` run their forward order on the forward half and their backward order on
the backward half. In `Pulling`, a label past the half-way point is stored but never pulled, so it
is the boundary label the join pairs.

An algorithm asked for a direction it does not support throws `std::invalid_argument`: when
`ResourceGraph::create_algorithm` builds it, or, for one you constructed yourself, when it solves
(`Algorithm::supported_directions()` lists what it supports). A backward search keeps its labels in
the container you pass, turned backward: `LabelList` and `LabelBuckets` each declare
`with_direction<TargetDirection>()`, an empty copy of themselves that compares in direction
`TargetDirection`. A container of your own that does not declare it is refused for a backward
search. `create_algorithm<Strategy>(params)` returns a
`std::unique_ptr<Algorithm<ResourceType, LabelContainerType>>`: the class that searches is built
from the strategy's parts, in the direction the params ask for.

**The model checks run first, always.** A backward or bidirectional solve runs the checks its
direction needs ([Model checks](#model-checks)) on the whole model before anything else: before any
preprocessing, whatever `preprocess` says, and also when `Algorithm::solve` is called directly, or by
an algorithm such as `Diversification` that runs another one inside it. So `preprocess` only says
whether the solve may reduce the graph; it never skips a check. A forward search needs no check.

## Model checks

A forward search reads nothing but the forward functions. A search that also extends labels
**backward**, from the sinks, reads each resource's backward semantics: its
`directional_resource_kind()`, its backward extension, start and feasibility test. A
**bidirectional** search also **joins** a forward and a backward half into one path, so it reads
each component's `join_rule()` as well. A model whose backward semantics are missing or incoherent
would let those searches return infeasible paths, or none, while reporting `complete`. The model
checks find that before any label exists:

| Check | Needed by | What it checks |
|---|---|---|
| `BackwardExtensionCheck` | backward and bidirectional searches | every component and the composition can extend, start, test and compare backward, and the backward values mean what the forward search enforces |
| `JoinCheck` | bidirectional searches | every component declares how two halves join, and the halves the join adds are sums |

`ResourceGraph::check_model(direction)` runs the checks a search in that direction needs, without
solving, and returns one line per problem (an empty `problems` list means the model is ready):

```cpp
const rcspp::ModelReport report = graph.check_model(rcspp::SearchDirection::Bidirectional);
for (const auto& problem : report.problems) {
    std::cerr << problem << '\n';
}
```

In Python, `rg.check_model("backward")` returns the list, for `"bidirectional"` by default.

The checks read the whole model, arcs that preprocessing removed included, so a verdict never
depends on what one solve's bound happened to remove. They never change the graph.
A backward or bidirectional solve runs them itself, before any preprocessing and whatever
`preprocess` says.
A model that fails them is refused with a `ModelRefused` (a `std::runtime_error`, so Python sees a
`RuntimeError`):

```text
a bidirectional search cannot run on this model:
  - component 1: arc 1 -> 2 consumes -3.000000, but its backward reading assumes no arc
    lowers the value: a backward label carries only a ceiling, so a path dipping below the
    floor would be accepted; loads must be non-negative
```

There are twenty-two complaints, in two groups. The last column says which searches each one
refuses: a backward search is refused for sixteen of them, a bidirectional search for twenty-one.

**Something was not declared, or was declared for the wrong kind of resource:**

| Complaint | What to do | Refuses |
|---|---|---|
| `no node carries a resource` | The checks read the declarations off a node's resource; give the nodes their resources. | both |
| `its extension function declares no directional_resource_kind()` | Derive the extension function from `DirectionalExtensionForm` or a form built on it (`resource/functions/extension/directional_extension_form.hpp` explains them), or declare `static constexpr DirectionalResourceKind kind`. | both |
| `its feasibility function declares no join_rule()` | Return a `JoinRule` from `join_rule()` — `AlwaysTrue` if the resource genuinely cannot block a join, and say why. | bidirectional |
| `its feasibility function's test cannot be read backward` | The component is not a threshold, and its feasibility function declares no `join_rule()`: in the library that means a test of what the path has collected *up to* a node (the table below), which a backward label, holding what the rest of the path collects, cannot answer.  A backward search would lose feasible paths or return infeasible ones.  The fixes are the ones in the table below. | both |
| `its feasibility function declares JoinRule::ValueOrder … on a resource that has none` | `ValueOrder` is `forward <= backward` on two scalar values.  A container resource has no such order: declare `JoinRule::Custom` and write the test. | bidirectional |
| `the model's composition dominance function cannot be read backward` | A composition dominance function you passed to `ResourceGraph`'s constructor overrides `check_dominance` only, which is all a forward solve needs.  Override `check_back_dominance` too, or keep the default `CompositionDominanceFunction`. | both |
| `the model's composition extension function cannot be read backward`, `… has no backward start`, `… feasibility function cannot be read backward` | The same for a composition extension or feasibility function: one that overrides only `extend` or `is_feasible` compiles and solves forward, but has no `extend_back`, `start_back` or `is_back_feasible`.  Override those too, or keep the defaults `CompositionExtensionFunction` and `CompositionFeasibilityFunction`. | both |
| `the model's composition feasibility function has no join test` | The same for `can_be_joined`.  A `can_be_joined` of your own must be exact, like a component's: no check can tell one that refuses too much. | bidirectional |
| `the model's cost function is not additive` | A backward label that reaches a source is a complete path, and the join prices a path as the forward half's cost plus the backward half's.  That is exact only when every component the cost reads has a zero cost (`TrivialCostFunction`) or a value cost (`ValueCostFunction`) under an accumulating extension.  A threshold's backward value is a deadline or a remaining capacity, not a cost, so a `CompositionCostFunction` over a time window with a `ValueCostFunction` is refused.  Give that component a `TrivialCostFunction`, or keep the default cost function, which reads component 0 only. | both |

The `join_rule()` one has **two causes, and only one of them is yours.** A function you wrote that
never answered is the first. (For a component that is not a threshold, the same declaration also
refuses a backward search, with `its feasibility function's test cannot be read backward`: a
backward search does not join, but its labels carry the same suffix.)
The second is a library function that answers `Unspecified` on
purpose, because the configuration you gave it has no backward reading: its test is a question
about what a label has collected *so far* — a predicate on a **prefix** — and a backward label
carries a **suffix**. Each of these would fail silently, returning an infeasible path or nothing at
all with a `complete` status, so they are refused instead:

| Component | When it refuses | What to do instead |
|---|---|---|
| `IntersectionFeasibilityFunction` | **required** values (`forbidden = false`) | Model the requirement on the whole path, not per node: there is no backward reading of "collected one of these already". |
| `IntersectionFeasibilityFunction` | **forbidden** values that are not `{v}` at node `v` | Use the ng-route condition, `forbidden_by_node[v] = {v}`, with `NgPathExtensionFunction`: see "One rule the join imposes" under [Bidirectional search](#bidirectional-search-directionbidirectional). |
| `SizeFeasibilityFunction` | a non-zero **minimum** size anywhere, or a **per-node cap** that differs from the default | Use one cap for every node, or solve forward.  A single cap is suffix-safe: a backward label that reaches a source holds the whole set, and every set along the path is a subset of it.  A floor or a per-node cap bounds what the path has collected *up to* a node, which a suffix cannot see. |
| `MinMaxFeasibilityFunction` | a non-zero **minimum** anywhere, default or per node, under a threshold extension (`CapacityExtensionFunction`, `TimeWindowExtensionFunction`) | The same fix, for the same reason: a backward label carries only a *ceiling*, so neither the join nor a backward label that reaches a source can tell whether the forward value got as high as the floor. |
| `MinMaxFeasibilityFunction` | under an accumulating extension (`AdditionExtensionFunction`), any window other than a single `[0, max]` | Use `CapacityExtensionFunction`.  A per-node window is the same prefix question as a per-node size cap, and a non-zero minimum rejects the empty suffix a backward label starts from; either way the sum of the two halves cannot be tested exactly. |
| `ReachableFeasibilityFunction` | always | Its `is_reachable` asks what the label has already collected, which a suffix cannot answer. |

In every one of these, forward-only solves are unaffected.

**Or two declarations were each legal on their own and incoherent together.** All eleven of these
are a *pairing* fault: nothing is wrong with either half in isolation, which is why only a check
that sees both catches them.

The clamps a threshold extension applies are not declared: the checks observe them, by running the
extension on values beyond every bound (the infinities, or an integral type's extremes) through an
arc leaving and an arc entering each node, so a threshold of your own is checked however it is
written, and must accept any value of its type.  The same goes for where a backward label starts at
a sink: the checks call `start_back` through an arc entering the sink, as a backward search does.
Each clamp and start is then put to the node's forward test, which must admit it and reject a
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
| `its backward labels at node … are not clamped at all, but its feasibility function rejects values above some bound there` | The extension's backward step does not clamp, so a deadline can stay above the node's upper bound and the backward search admits deadlines the forward search rejects — under a time window's backward test, which reads only the opening time, the join or a backward label reaching a source would accept an infeasible path.  Clamp each backward label to its node's upper bound, as `ThresholdExtensionForm` does, built from the feasibility function's `NodeBounds`. | both |
| `its backward labels at sink … start at the type default, but its feasibility function rejects values above some bound there` | The extension's `start_back` sets nothing, so a backward label at the sink keeps the type default, 0, as its deadline, and the backward search rejects deadlines a forward path meets.  Start each backward label at the sink's upper bound, as `ThresholdExtensionForm::start_back` does. | both |
| `its backward labels at node … are clamped to …` (or `at sink … start at …`) `, which its feasibility function rejects there` | A threshold extension clamps each backward label to its own upper bound at the node.  If the node's `is_back_feasible` rejects that value, every backward label there is lost: the extension was built with other caps than the feasibility function, typically `CapacityExtensionFunction(capacity)` beside per-node caps on the feasibility function.  Build both from one `NodeBounds` (`make_node_bounds(0, capacity, per_node)`, or `feasibility->bounds()`), or use `presets::add_capacity_resource`. | both |
| `its backward labels at node … are clamped to …` (or `at sink … start at …`) `, above` (or `below`) `the largest value its feasibility function admits there` | The same mismatch where the node's backward test cannot see it.  The node's ceiling is the largest value its forward test admits.  A clamp or start *above* it lets the backward search admit deadlines the forward search rejects — a time window's backward test reads only the opening time, so the join would accept infeasible paths; one *below* it rejects deadlines a forward path meets.  The fix is the one above.  A forward test of your own that admits values a little above its ceiling, beyond the library's own `epsilon` tolerance, reads as a ceiling higher than the clamp and is refused: make it exact. | both |
| `a forward label leaves source … at …, below the lower bound its backward reading enforces there` | A forward label starts at a source with that node's resource value (0 unless you gave the node one), and no forward step raises it to the source's opening time.  A backward label that reaches the source with a deadline below the opening is marked unmeetable, although the forward search departs earlier and meets it, so a backward search would miss paths the forward search finds.  Open the source's window at the start value or earlier.  A bidirectional search is not refused: it finds such paths through its forward half and the join. | backward |
| `arc … consumes …, but its backward reading assumes no arc lowers the value` | A backward label carries only a ceiling, so it cannot see a path's value dip below a floor inside the suffix: with a negative load, a `MinMaxFeasibilityFunction(0, capacity)` under `CapacityExtensionFunction` or `AdditionExtensionFunction` accepts a path whose load went below 0.  Loads must be non-negative.  A time window is exempt, since it waits at each node's opening time. | both |
| `its extension declares DirectionalResourceKind::Accumulate, but along … it is not a sum` | The join adds the component's two halves, through its cost (`ValueCostFunction`) or its feasibility function's join test (`MinMaxFeasibilityFunction` under an accumulation tests `forward + backward`), which is exact only if a path's value is the sum of one amount per arc.  `Accumulate` promises less: that the step is the same in both directions.  A bottleneck, the largest load seen, is such a step, but its halves 3 and 5 add up to 8 where the path's value is 5.  The check runs the extension: from the type default, two steps along the model's arcs must give the sum of the two single steps.  Write the step as an addition, or keep the component's halves apart: a `TrivialCostFunction`, and a feasibility function whose join test does not add. | bidirectional |
| `its extension declares DirectionalResourceKind::Accumulate, but starts its backward labels at sink …` | A forward label starts at the type default, so a backward label whose halves are added must start there too: any other start is counted on top of the path's own sum.  Leave `start_back` to its default. | bidirectional |
| `its feasibility function forbids each node at itself … does not declare DirectionalResourceKind::ArcEndpoints` | The feasibility test asks *am I already in my own memory*, which needs the memory at a node to exclude that node in **both** directions.  Only an `ArcEndpoints` extension gives that.  A `DirectionalResourceKind::ArcValue` container takes its elements from the arc's *value*, which is the same object going each way, so the backward label reaches a node already holding it and is rejected there — every time.  Use `NgPathExtensionFunction`, or `presets::add_elementary_resource` for an elementary path. | both |

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

<!-- docs-check: given double capacity = 10.0;
     std::map<size_t, std::pair<double, double>> per_node_caps; -->

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
bp.bucket_width          = 200    # each bucket spans 200 units of time
bp.bucket_resource_index = 0      # resource 0 is time
bp.sort_resource_index   = 1      # sort within bucket by resource 1

result = rg.solve(algorithm="simple", params=bp)
```

In C++:

<!-- docs-check: given ResourceGraph<RealResource> graph; -->

```cpp
using Resources = ResourceTypeComposition<RealResource>;  // the graph's resource types
// bucket on real component 0 (time), sort within a bucket on real component 1
using Buckets = LabelBuckets<RealResource, RealResource, Resources>;
AlgorithmParams<Buckets> params(Buckets(/*bucket_width=*/200, /*bucket_resource_index=*/0,
                                        /*sort_resource_index=*/1));
auto result = graph.solve<strategy::Simple, Buckets>(ub, params);
```

## How the labelling algorithms are built

The labelling searches (`Simple`, `AStar`, `Pushing` and `Pulling`, in every direction) share one
kernel, in `cpp/rcspp/algorithm/labelling/`. A strategy is not a class with its own loop but a
*descriptor*: a struct in `rcspp::strategy` that names its parts. The parts a descriptor may name
are public, in `rcspp::parts`; the code that runs them is internal (`rcspp::detail`).

- **The search** holds one direction's labels, one container per node, and is the only code that
  extends them. Extending a label along an arc checks that the arc mask leaves the arc in and that
  the arc is reachable, extends the label, tests its feasibility, asks the admission rule, and
  stores the new label and hands it to the schedule. Every outcome (masked, unreachable,
  infeasible, rejected, accepted) goes to the search's observer, which by default records
  nothing.
- **Admission and retention.** The admission rule (`parts::DominanceAdmission`) decides whether a
  new label is kept at its node: under exact dominance, when no label there dominates it, and then
  the labels it dominates are removed. The retention (`parts::KeepAtNodes`) decides where kept
  labels live: in their node's container, until dominated or until the solve ends, when the labels
  at the end nodes are read as the solutions.
- **Schedules** decide the order labels are taken in. `parts::FifoSchedule` (`Simple`) hands them
  out first in, first out; `parts::BestFirstSchedule` (`AStar`) lowest cost plus completion bound
  first; `parts::SweepSchedule` (`Pushing`) node by node; `parts::PullSchedule` (`Pulling`) visits
  one node at a time and pulls into it from its neighbours. A label schedule
  (`parts::LabelSchedule`: FIFO, best-first, the sweep) hands the step one label, which it extends
  along every arc leaving its node. A pulling schedule (`parts::PullingSchedule`: `PullSchedule`)
  has the step visit one node and extend into it the labels waiting at its predecessors. A
  schedule also applies the per-node
  quota (`max_extended_labels_per_node`) and trims itself under memory pressure. What it cuts is
  stored aside for the next phase in a single-direction search, and abandoned (left in its
  container, never extended, and counted) on a side of a bidirectional one, which runs one phase.
- **The take rule** is the checks a label goes through when it is taken, written once for every
  search. First come those whose answer can change while the label waits: dominated since it was
  queued, the prune against the best cost found so far. Then come those whose answer is fixed
  when the label is created: a label at an end node lowers that best cost and may be recorded; one
  of infinite cost
  is dropped; on a side of a bidirectional search, one past the half-way point is kept but not
  extended.
- **The step** is one unit of a search's work: take a label, run the take rule, then extend the
  label along every arc its direction walks, or drop it as the rule says. With a pulling schedule,
  it pulls into the next node and runs the rule on each new label.
- **Two classes run the searches.** One runs a single search, forward or backward: the loop, the
  memory checks and the phases, and the completion bound when its schedule's order reads one. The
  other runs two, one per direction, sharing one label pool and one best cost; its interleave
  (`parts::LeastWorkFirst`) takes the next unit of work from the side that has tried fewer
  extensions,
  and the join pairs the two halves at the end. `create_algorithm<Strategy>(params)` builds the one
  `params.direction` asks for, from the parts the descriptor names in each direction.

`Simple` names only its schedule; every other part is the exact search's:

<!-- docs-check: file-scope -->

```cpp
namespace my {
// strategy::Simple, as the library writes it.
struct Simple {
        template <typename ResourceType, typename Direction>
        using schedule = rcspp::parts::FifoSchedule<ResourceType>;
};
}  // namespace my
```

## A strategy of one's own

A search the library does not name is a descriptor of your own, from the library's parts or from
parts of your own, solved as a built-in strategy is: `graph.solve<MyStrategy>(params)`. Every member
but the schedule is optional. The member templates (`schedule`, `admission`, `retention`,
`observer`) are given the resource type and the direction, so a strategy can choose a different
part forward and backward, and a bidirectional search gets a different part on each side;
`interleave` is a plain type. A plain `using admission = ...;` (or retention, observer) is refused
at compile time: it would not be read.

| Member | What it decides | Built in (`rcspp::parts`) | Concept |
|---|---|---|---|
| `schedule<ResourceType, Direction>` | the order labels are taken in, the per-node quota, the trim under memory pressure (required) | `FifoSchedule`, `BestFirstSchedule<R, Key>` with `CostKey` or `CostPlusBoundKey`, `SweepSchedule`, `PullSchedule`, `DepthFirstSchedule<R, Key, Filter>` with `KeepAllChildren` or `TabuChildFilter` | `Schedule`, and either `LabelSchedule` (hands the step one label at a time) or `PullingSchedule` (one node at a time, as `PullSchedule` does) |
| `admission<ResourceType, Direction>` | whether a new label is kept at its node | `DominanceAdmission` (default), `AdmitAll` | `Admission` |
| `retention<ResourceType, Direction>` | where kept labels live, as `template <typename ContainerType> using type` (`Retain<RetentionTemplate>` names a library one) | `KeepAtNodes` (default), `KeepWhileWaiting` | `Retention` |
| `observer<ResourceType, Direction>` | told of every extension | `NoObserver` (default) | `Observer` |
| `interleave` | which side of a bidirectional search works next | `LeastWorkFirst` (default) | `Interleave` |
| `driver` | runs the search several times in one solve, changing it between runs (forward or backward; not bidirectionally) | none (default: one run, of up to `max_phases` phases), `TabuEpisodes`, `ImprovingTabuEpisodes` | `Driver` |
| `backward`, `bidirectional` | whether the strategy searches in that direction | `true` (default) | |

This strategy takes labels best first forward and first in, first out backward; a bidirectional
solve then runs a best-first forward side and a FIFO backward side:

<!-- docs-check: file-scope -->

```cpp
struct BestFirstForward {
        template <typename ResourceType, typename Direction>
        using schedule = std::conditional_t<Direction::backward, rcspp::parts::FifoSchedule<ResourceType>,
                                            rcspp::parts::BestFirstSchedule<ResourceType>>;
};
```

This one keeps Simple's order and replaces the admission rule with a rule that keeps at most three
labels per node. It is not exact, and says so:

<!-- docs-check: file-scope -->

```cpp
struct AtMostThree {
        static bool is_exact(const rcspp::parts::AdmissionContext& /*context*/) { return false; }

        template <typename ResourceType, typename NodeView>
        bool admit(const rcspp::Label<ResourceType>& candidate, NodeView& node,
                   const rcspp::parts::AdmissionContext& /*context*/) const {
            if (node.is_dominated(candidate) || node.size() >= 3) {
                return false;
            }
            node.evict_dominated_by(candidate);
            return true;
        }
};

struct SimpleAtMostThree {
        template <typename ResourceType, typename Direction>
        using schedule = rcspp::parts::FifoSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = AtMostThree;
        static constexpr bool bidirectional = false;  // forward and backward only
};
```

This one is a dive: depth first, keeping every feasible label until it is extended, and storing
none at the nodes. `DepthFirstSchedule` places a label's children together once it is extended,
and hands them out next, cheapest first
(its key), each child's subtree before its next sibling; its filter (`KeepAllChildren` by default)
may drop some first. `AdmitAll` keeps every label without comparing it, and `KeepWhileWaiting`
stores none at the nodes: a solution is recorded when its label at the sink is taken, and each
label is released once extended, living on while a label extended from it waits. A dive searches
forward or backward; this one, like Greedy, says it does not search bidirectionally:

<!-- docs-check: file-scope -->

```cpp
struct Dive {
        template <typename ResourceType, typename Direction>
        using schedule = rcspp::parts::DepthFirstSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = rcspp::parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = rcspp::parts::Retain<rcspp::parts::KeepWhileWaiting>;
        static constexpr bool bidirectional = false;
};
```

A **driver** runs the search several times in one solve, each run up to `max_phases` phases. It
is built from the params. `begin(search)` is called at the start of each solve and returns
whether to run at all; `after_run(search, number_of_runs)` is called after each run and returns
whether to run again. Between them it reads how the run ended (`search.last_run()`, a
`parts::RunEnd`):

- whether it recorded a path (`recorded`), with the path's cost and arcs, and whether the path
  was new;
- whether no label was left (`exhausted`);
- whether labels the per-node quota or memory pressure stored aside were left after the last
  phase (`ended_with_stored_aside`).

A run with none of these was stopped by the time limit, an interrupt or the memory limit. The driver
may change the search: use an arc mask of its own (`use_arc_mask`, which replaces the mask
`Algorithm::set_arc_mask` set; merge `get_arc_mask()` into it to keep the caller's hard arcs), end
each run at its first path (`end_runs_at_end_node`), or start the next run from the start labels
(`reseed`). A driver that changes the search loses the proof, and says so with
`lose_proof(rcspp::ProofLoss::HeuristicDriver)`. A run counts no iterations: a driver that reads
`max_iterations` counts its runs against it. This one runs a dive until it finds nothing, masking
each path it finds:

<!-- docs-check: file-scope -->

```cpp
struct MaskEachPath {
        explicit MaskEachPath(const rcspp::AlgorithmBaseParams& /*params*/) {}

        template <typename SearchType>
        bool begin(SearchType& search) {
            mask.reset(search.graph().get_arc_id_bound());
            search.use_arc_mask(&mask);
            search.end_runs_at_end_node();
            return true;
        }

        template <typename SearchType>
        bool after_run(SearchType& search, size_t /*number_of_runs*/) {
            if (!search.last_run().recorded) {
                return false;  // nothing left to find
            }
            search.lose_proof(rcspp::ProofLoss::HeuristicDriver);
            for (const size_t arc_id : search.last_run().arc_ids) {
                mask.set(arc_id, rcspp::ArcMask::State::Hard);
            }
            search.reseed();
            return true;
        }

        rcspp::ArcMask mask;
};

struct DiveEachPathOnce {
        template <typename ResourceType, typename Direction>
        using schedule = rcspp::parts::DepthFirstSchedule<ResourceType>;
        template <typename ResourceType, typename Direction>
        using admission = rcspp::parts::AdmitAll;
        template <typename ResourceType, typename Direction>
        using retention = rcspp::parts::Retain<rcspp::parts::KeepWhileWaiting>;
        using driver = MaskEachPath;
        static constexpr bool backward = false;
};
```

`strategy::Tabu` is such a strategy: Greedy's parts, with `parts::TabuChildFilter` as the
schedule's filter (it drops a child reached along a soft arc unless every child is) and
`parts::TabuEpisodes` as its driver, which makes each path's arcs soft between episodes.
`strategy::ImprovingTabu` names `parts::ImprovingTabuEpisodes` instead.

**The contract.** `create_algorithm` checks every part of a descriptor against its concept before it
builds anything, and names the first that fails. Three rules the concepts cannot check are part of
the contract:

- **An admission rule says whether it is exact.** `is_exact(context)` answers `true` only for a rule that
  keeps every label exact dominance keeps; a search whose rule is not exact is not a proof.
- **A schedule loses no label without a trace.** Every label pushed is handed out; released
  because it was found dominated, or evicted; stored aside through `parts::StoredAsideLabels`, so
  that it waits again in the next phase and the status reads it (or, on a bidirectional side,
  abandoned and counted); or released under memory pressure, which the solve reports.
- **A part touches only what its interface gives it.** An admission rule reads and evicts through
  the node view (`is_dominated`, `evict_dominated_by`, `size`); a schedule releases through the pool
  and erases through the callable it is handed. The reference counts depend on it.

**Testing a strategy.** `rcspp::test_support::optimum_problem<Strategy>(graph, params)`
(`rcspp/test_support/optimum_problem.hpp`) solves `graph` with the strategy in every direction it
searches, and returns `""` when each finds forward Simple's optimum (or, like it, nothing) along
paths that replay through the model, and otherwise what went wrong first. An exact strategy must
pass on every model:

<!-- docs-check: skip -->

```cpp
EXPECT_EQ(rcspp::test_support::optimum_problem<BestFirstForward>(graph, params), "");
```

**What stays internal.** The take rule, the step, the classes that run the searches, the completion
bound, the half-way point and its checks, and the join are the same for every strategy, and are
where correctness lives; they are not parts, and a strategy cannot replace them.

**From Python.** A descriptor is a C++ type, so a strategy of your own is a C++ strategy. Python
offers the built-in strategies by name; one that should be reachable from Python is a line of the
bindings' dispatch table.
