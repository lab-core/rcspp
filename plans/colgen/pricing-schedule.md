# Pricing schedule (`T-2.1`)

Design chat started 2026-11-27 (main line, `D-307`). Not implemented; code goes in `../rcspp-dev` when it is.
Decisions `D-450`-`D-455`, findings `F-190`-`F-196`, questions `Q-80`-`Q-83`. Frame: `phase-2-design.md` §1.

## 1. Why

| point | detail |
|---|---|
| pricing is most of a run | on R2 nearly all of it; the master LP is cheap next to it |
| only the last call must be exact | before it, any negative column will do; the last one must finish exact to prove the LP |
| the danger | a cut or heuristic call that finds nothing taken as a proof stops at a wrong LP: #49's RC201_50 stopped at 1,860.45, not 535.29 |
| three answers bundled | the budget (#49: hard exact calls), several pricers (`D-27`: skip some per round), parallel (`D-306`: the colleagues' ask) |

## 2. What exists (the package, 2026-11-27)

| piece | where |
|---|---|
| stages, cheapest to exact; stop at the first that finds columns, proves, or runs out of time | `graph_pricer.py:235-286` |
| every pricer, one after another, every iteration; proven when all prove | `column_generation_loop.py:436-517` |
| `PricingRequest` (threshold, `timeout_s`), `PricingResult` (columns, status, bound, `exact_stage`) | `pricer.py` |
| one status for any early stop (`SolverLimit`: time, labels, solutions alike) | `pricer.py:29` |
| #49's budget: x`growth` on a timeout with nothing, up to `max_seconds`, `sticky`; restart, not resume | `origin/feature/pricing-budget`, `examples/cpp/vrp/pricing_budget.hpp` |

Prior art: `prior-art.md`, the comparison table's `PricingStrategy` row and `PA-03`, `PA-07`. Stages and a "complete"
flag are everywhere (GCG, Coluna, RouteOpt, vrpy); GCG adds pricer sorting, chunks and an eager round every N; #49's
escalation is a careful form of RouteOpt's time limit with a complete flag; parallel is rarer in open frameworks.

## 3. The model: a schedule tree (`D-450`)

Built vertically; blocks nest freely (the user, 2026-11-27).

| block | does | notes |
|---|---|---|
| **Search** (leaf) | one algorithm with its params on a pricer's graph | the only leaf; tabu, greedy, exact, a stage of one's own |
| **Sequence** | children in order; the next only when the previous found nothing | today's stages are a `Sequence` |
| **Parallel** | children at once; stop and merge rules | `Parallel` inside `Sequence` inside `Parallel`, any depth |
| **Budget** | wraps any block: limits, escalation, what happens at the cap | wraps a leaf, a composite, maybe a phase |
| **Phase** | a block and the conditions that end it; phases in order | the top level: phases only, or any block (`Q-80`) |

The user's nesting example: `Parallel(A = simple, B = Sequence(tabu, Parallel(greedy, greedy), exact))`.

The user's run example, as a sketch (not an API):

```python
PricingSchedule([
    Phase(until=Iterations(10),  pricing=Sequence([tabu, exact])),
    Phase(until=RunSeconds(300), pricing=Sequence([Parallel([tabu, greedy]), exact])),
    Phase(until=None,            pricing=Sequence([greedy, tabu,
                                     Budget(exact, start_s=50, growth=2, max_s=900, at_cap="fail")])),
])
```

- **Limits at every level** (`D-453`): search, block, pricing call, phase, run; in seconds, labels, columns,
  iterations. The run's own (`max_iterations`, `timeout_s`) exist.
- **Phase conditions:** iteration count, run time, phase time, gap (`T-2.3`'s bound), LP stagnation over k
  iterations, columns found last round, an exact call that failed to prove; several joined by "or" first.
- **Pricers:** a leaf runs on every pricer by default, or on a named subset; `Parallel` over searches, pricers or both.
  GCG's options (`D-27`) become phase conditions and subsets.
- **A sequence need not end exact** (the user): the proof is a property of the whole schedule, checked by the verifier
  (§4), not of each sequence.
- Today's stages map to `Sequence`, today's loop to the default schedule: defaults change nothing.

## 4. Verifier: visitors over the tree (`D-452`)

The user's idea: one walk pattern (visitor) over every node, answering questions about the schedule.

| visitor | answers |
|---|---|
| exactness | is the run guaranteed to end proven (every pricer reaches a finished exact leaf when nothing is found, in the phase never left, under limits that cannot cut it)? or why not |
| validation | refuses nonsense before running (a tuner samples it, §8): an empty block, a cap below its start, a phase that cannot end |
| parameter space | the tuner's flat, named knobs (`phase.2.budget.growth`) |
| serialization | to and from TOML or a flat dict (benchmark and tuner share it) |
| statistics | names of the leaves and phases for the records (`T-2.5`) |

Proposed: the proof itself stays in the loop (an iteration proves only when every pricer's call ended at a finished
exact leaf with nothing); the exactness visitor only predicts it. "Fail if the cap is reached" is a status of its own.

Performance (the user's worry): the visitors run once, when the schedule is built; per iteration the loop walks a
few nodes, microseconds against milliseconds of pricing. To check as every task does: the default schedule costs
nothing measurable against today's loop.

## 5. Limits and the label cap (`D-453`)

- **Total label cap per solve:** none exists (`F-191`). Proposed `max_extended_labels` in `SolveLimitParams` (#120),
  bound in Python. Labels are machine-independent (`algorithm.hpp:256`): reproducible budgets and phase lengths.
- Each unit at each level of §3; a child's limit is capped by what its parents have left.

## 6. The budget: restart or resume (`Q-81`)

- **Restart** (#49): works with any pricer, today. The cut attempts cost up to `1/(growth-1)` of the last: 11 % at
  x10, 100 % at x2.
- **Resume:** continue the cut search where it stopped; the cut attempts cost nothing, so small growth factors are free
  (the tuner will want them). `Algorithm::solve` always initializes again (`F-194`); the state is mostly kept.

| resume needs | how |
|---|---|
| a kept algorithm in Python | #120's `Solver` (`F-190`): add `resume(limits)` |
| the solutions found before | kept, not moved out at the end of `solve` |
| the graph unchanged | duals equal during an escalation; refuse a stale resume (a graph version) |
| the shortest-path reduction | undone after each search today: kept until the search is done |
| bidirectional | the join again over the whole label sets (#43's join after an early stop) |
| memory | a paused search holds its labels; `release()` or `max_retained_labels` (#119) |
| heuristics | tabu, greedy continue their own episodes |

Also: paused searches can take turns on one thread, label slices each: a deterministic race.
Open: a budget around a composite (scale every child's limit, or resume the leaf that was cut).

## 7. Parallel

- Python threads: the solve releases the GIL (`D-306`). A graph clone per concurrent search, kept between iterations,
  each updated with the duals; #115 makes a clone keep the sources' order (`F-195`).
- "Stop the others": a stop flag per solve, an atomic in C++ bound to Python; today Python's `should_stop` is only
  Ctrl-C (`F-192`).
- Proposed default: wait for all, merge in a fixed order (deterministic); "first wins" opt-in (`Q-83`).
- Merge rules, each replaceable: duplicates by node sequence, k best per search, a global cap; columns keep their
  pricer's index.

## 8. For the tuner (`D-451`, `T-1.37`)

| point | why |
|---|---|
| named shapes (categorical) with numeric knobs | configurators handle conditional parameters, not trees |
| one serial format (§4) | a tuned result is a file the runner runs |
| label limits beside seconds (§5) | time is noisy between machines and runs |
| deterministic parallel by default (§7) | same knobs, same iterations |
| cheap strict validation (§4) | sampled nonsense fails before running |
| unambiguous statuses | the objective: time to a proven LP, unproven and failed runs penalized |

## 9. Open PRs that help (checked 2026-11-27)

| PR | gives | for |
|---|---|---|
| #120 (on #117-#119) | `Solver`: one kept algorithm per graph, per-solve `SolveLimitParams` (`F-190`) | leaves, budget, resume |
| #119 | labels reused between solves; `max_retained_labels` | memory of kept and paused searches |
| #83 | `SolveResult.proven`, `proof_lost_by` (`F-193`) | the exact leaf's proof, in place of the pricer's own |
| #85, #86 | `prune_by_threshold`, backward greedy and tabu, `bound_guided_greedy` (`F-196`) | more leaf kinds |
| #43 | join budgets, joining after an early stop | bidirectional resume, `JoinTruncated` |
| #115 | clone keeps source and sink order (`F-195`) | deterministic parallel clones |
| #49 | the budget's rule, its RC201_50 case | the restart mode and the done-when |

All pamun1's, open; the heuristics (#81-#87) and performance (#117-#121) stacks are not merged. `T-2.1`'s engine parts
go on top of them, not beside (`Q-81`).

## 10. Order and split (`D-454`)

Each feature designed alone, then the bundle (the user). Proposed split into tasks:

1. leaves and limits, the label cap (engine, on #120);
2. the budget: restart, then resume (engine search handle);
3. parallel: stop flag, clones, merge, determinism;
4. phases and conditions, the proof and the visitors;
5. several pricers: subsets, GCG's options;
6. the bundle: API, serial format, tuner's view, statistics, the three layers (`D-308`; the C++ example layer may get a
   subset, `Q-82`).

Side idea, not now: a horizon-limited heuristic stage (`T-2.8`, `D-455`).
