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

### The critical resource

One resource acts as the **clock** that says where "half-way" is.  It must be:

- **monotone** — never decreasing along an arc, so its value crosses `H` exactly
  once along any path;
- **bounded**, with a known finite range;
- **a threshold backwards** — its backward form must carry a *ceiling* on the
  forward scale (the latest arrival still admissible here), not the consumption
  from here to the sink.  `TimeWindowExtensionFunction` and
  `BudgetExtensionFunction` are thresholds; `AdditionExtensionFunction` is not.
- **inside the dominance order, increasing** — the resource's `DominanceFunction` must say that a
  smaller value dominates a larger one. `ValueDominanceFunction` does; `TrivialDominanceFunction`
  does not. Without it a dominating label can sit *above* the half-way point while the label it
  evicted sat below, and the path through the evicted label is lost.

Cost is never a valid clock: reduced costs go negative during column generation,
which breaks monotonicity.

If the chosen resource fails any of these, **the half-way bound is switched off**
and a `LOG_DEBUG` line says why.  The solve stays correct — both directions simply
run to completion and every pair is considered — it is only slower.  A *wrong*
bound would be far worse: it can discard a valid path in both directions and then
report `complete`.

`critical_resource_index` is an index within the **cost resource type's** slot, so
the clock must share that type (a real-valued time alongside a real-valued cost is
the usual case).  A model whose clock is an `int` resource while its cost is
`real` needs the C++ API.

### What the model must declare

Every resource in the model must declare how it behaves backwards.  A solve whose
model does not raises **before the first label is created**, naming every offending
component and what is wrong with it:

```text
BidirectionalDominanceAlgorithm cannot run on this model:
  - component 1: its extension accumulates but its feasibility function seeds a backward
    label at the far end of its range, so the first backward extension leaves that range;
    use a Threshold extension (e.g. BudgetExtensionFunction) instead
```

There are four complaints it can make, and they fall into two pairs.  **Something was not
declared at all:**

| Complaint | What to do |
|---|---|
| `its extension function declares no backward_kind()` | Derive the extension function from a form in `resource/functions/extension/backward_form.hpp`, or declare `static constexpr BackwardKind kind`. |
| `its feasibility function declares no merge_rule()` | Return a `MergeRule` from `merge_rule()` — `AlwaysTrue` if the resource genuinely cannot block a join, and say why. |

**Or two declarations were each legal on their own and incoherent together.**  Both of these
are a *pairing* fault: nothing is wrong with either half in isolation, which is why only a check
that sees both catches them.

| Complaint | What it means |
|---|---|
| `its extension accumulates but its feasibility function declares MergeRule::DominanceOrder` | `DominanceOrder` compares the forward value against the backward one, which reads the backward value as a *bound*.  Under an accumulation it is not a bound, it is the suffix's own consumption, so the comparison tests nothing the model contains — and it fails by *accepting*.  Use a threshold extension, or declare `MergeRule::Custom` with a body that adds the two halves. |
| `its extension accumulates but its feasibility function seeds a backward label at the far end of its range` | The seed says "start at the bound and count down" while the extension adds, so the label leaves its range on the first arc and the backward search silently finds nothing.  Use a threshold extension. |

(The messages are generated by `BidirectionalDominanceAlgorithm::describe_problem`; if you are
editing one, edit both.)

Both pairing faults have the same usual cause, a **capacity written as an addition**: `AdditionExtensionFunction`
with `MinMaxFeasibilityFunction(0, capacity)` counts *up* from zero while the
backward seed says "start at the bound and count *down*".  That is fine
forward-only and incoherent backwards, so it is refused rather than solved wrongly.
Use `BudgetExtensionFunction`, which subtracts:

```python
from rcspp.resource import BudgetExtensionFunction, MinMaxFeasibilityFunction

rg.add_real_resource(
    BudgetExtensionFunction(),                 # additive forward, threshold backward
    MinMaxFeasibilityFunction(0.0, capacity),
    TrivialCostFunction(),
    ValueDominanceFunction(),
)
```

`BudgetExtensionFunction` is available for the signed numerical types (`"real"`
and `"int"`) only — extending backwards subtracts, and on an unsigned type that
wraps to a huge positive value that reads as a very loose bound.

#### C++ callers get this at compile time

From C++, `add_resource` has a type-aware overload that is selected automatically
whenever the four function objects are constructed inline — which is every ordinary
call site.  It checks the same two things the setup validation checks, but as
`static_assert`s:

- the extension function declares a `BackwardKind` other than `Unspecified`;
- an `Accumulate` extension is not paired with a feasibility function that *always*
  seeds its backward label at a ceiling.

So the capacity-written-as-an-addition mistake above is a **compile error** in C++,
naming the fix in the message, rather than a `runtime_error` at the first solve.

Two things this does not change.  **Python is unaffected**: the bindings call the
type-erased overload, so a Python model is still checked at setup, with the same
message and the same component numbering.  And the runtime validation is still the
only check for `MinMaxFeasibilityFunction`, whose seed end is chosen from a
constructor argument and therefore cannot be read from the type — `seeds_itself_out_of_range`
asks the sharper question anyway, so it stays.  A C++ caller who wants the runtime
path (to test it, say) gets it by declaring one argument as a base-typed
`std::unique_ptr<ExtensionFunction<R>>` local.

### One rule the join imposes, and what it requires of a container resource

A container resource can declare that two halves may only be merged when their remembered sets are
**disjoint** — which is what `IntersectionFeasibilityFunction` with forbidden values declares, and
what the ng-path relaxation means.

That test is exact only when **the set a label stores is the memory it will carry out of the node it
sits on** — because at the join a forward half and a backward half compare their memories at the
node where they meet, and the forward half's must already have been filtered by that node. Two ways
to satisfy it:

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
extension narrow on arrival as above, or give it a `merge_rule()` of `AlwaysTrue`; do not give it
forbidden sets it does not mean.

`SizeFeasibilityFunction` shows the other half of the same question. Its merge test counts
`|forward ∪ backward|` — the union, because two halves that meet have both collected whatever they
share and the merged path holds it once — against the **tightest** cap in the model rather than the
join node's, because the merged path has to fit under the cap at every node it reaches after the
join and nothing else checks those. For a container that only grows, that count is the one at the
sink and bounds it everywhere earlier, so with a single global cap the test is exact.

With **per-node** caps it is sound but strict: a cap on a node the merged path never visits still
gates the join, and `can_be_merged` sees two values, not the suffix's nodes. So the rule declares
itself inexact — `merge_refusal_may_be_conservative()` — and the join replays those refused splices
through the real extension functions before dropping them. You pay for that only on refusals, only
under per-node caps; every other rule declares itself exact and the join takes it at its word.

That declaration is the hook to reach for if you write a merge rule that cannot decide its own
question exactly: say so, and be verified, rather than over-rejecting quietly.

### Parameters that behave differently here

| Parameter | Under `simple` / `pushing` / `pulling` / `astar` | Under `bidirectional` |
|---|---|---|
| `return_dominated_solutions` | a path reaching a sink is recorded immediately, so paths later dominated are still returned | the same, in both directions: a forward path reaching a sink and a backward path reaching a source are both recorded when they are found |
| `stop_after_X_solutions` | stops the search once that many solutions exist | stops the *search* the same way, but never truncates the join: the join runs to completion and the result list is resized afterwards, so a `complete` status still means the search was exhaustive |
| `prune_based_on_upper_bound_` | drops a label whose own cost is at or above the incumbent — valid for a complete path, not for a frontier | drops a half whose cost *plus a lower bound on its completion* is at or above the incumbent, and drops a join that is no better than the incumbent. With it off **and a finite `upper_bound`**, every join that bound admits is returned — which is what a pricing pool wants. With no `upper_bound` at all the join keeps pruning against the incumbent whatever the flag says: see below |
| `timeout_s`, `should_stop`, the memory limit | stop the search; whatever was found is returned | stop the search **and skip the join**. The join is the most expensive single pass here — 3 million pairs and 12.2 s on a full C201 — so running it after a stop has fired is the opposite of what the stop asked for. The status already says the run was cut short, and `number_of_joined_paths` is then 0. Note the asymmetry with the row above: a solution budget caps what is *returned* and leaves the search exhaustive, while these three mean the search did not finish |

Why the last row depends on `upper_bound` being finite: a solve that supplies none has expressed no
filter, and returning every admissible pair there is nobody's question.  Measured on the full C201
instance, doing so produced **3 065 288 solutions in 12.2 s** where the forward search finished the
same optimum in 0.68 s — while the bidirectional *search* extended 1.46× **fewer** labels.  On
RC201 the same run reached 9.3 GB resident.  None of that is search cost; it is recording and
de-duplicating the set.  So the incumbent cutoff stays on when there is no bound, and the flag is
honoured where it means something.

Note the last row: the bidirectional search's solution *set* is still generally smaller than the
forward search's, because the half-way bound stops forward labels before they reach a sink and the
join only produces paths whose clock crosses `H`.  The optimum is unaffected.  If you need every
column rather than the best one, measure both before choosing.

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

### When it pays, and when it does not

What decides this is **how many labels dominance has to sift at each node**, not how
long the horizon is and not how tight the windows are.  Dominance compares a new
label against every label held at its node, so halving the set at each node makes
each comparison cheaper as well as making fewer of them — which is why the
wall-clock gain, where there is one, is larger than the reduction in label
extensions.

Measured in a Release build across all six Solomon families, nineteen instances,
every one run to completion and every one agreeing on the optimum (see
`analysis/bidirectional-results.md` §3.3 for the full table).  What predicts the
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
  walks every arc, costs more than the halving saves.
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
