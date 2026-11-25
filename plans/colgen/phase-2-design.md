# Phase 2 design

The design chat of 2026-11-25 (`D-307`): the frame of every phase 2 task, `T-2.3` designed in full, and the open
questions of the others, each left to its own design chat. Decisions: `D-308`-`D-313`.

## 1. The frame

| rule | source |
|---|---|
| A component where methods differ, built as is where one answer exists | `D-274` |
| Every task covers the three layers unless decided otherwise: the package, the C++ example layer (its loop, bindings, stubs and `cpp_column_generation` package; the `cpp-native` program follows), the benchmark's options in the three runners. Mostly the package's code copied and reshaped for the language. The VRP examples stay as they are | `D-308` |
| Each task complete on its own: code, tests, docs page, `OPTIONS.md`, CHANGELOG in the same task | `D-308` |
| The loop exists in two places: the package's `column_generation_loop.py` and the example's `column_generation.cpp` (the example's Python loop goes) | `D-309` |
| Code and docs in ASCII only (`-inf`, not the infinity sign; `<=`, not the sign) | the user, 2026-11-25 |

## 2. `T-2.3`: the Lagrangian bound and the early stops

### 2.1 The bound (`D-310`)

At each iteration, after pricing the RMP's duals `pi` (master seen as a minimization, as `objective` is):

```text
LB = z_RMP + sum over pricers k of  U_k * min(0, b_k - d_k)
```

| symbol | what | where it comes from |
|---|---|---|
| `z_RMP` | the RMP's LP value | `MasterSolver.objective()` |
| `b_k` | pricer `k`'s bound on its best path's reduced cost, without the convexity dual | `PricingResult.min_reduced_cost_bound` (package `pricer.py:54`; C++ `pricer.hpp:40`) |
| `d_k` | the dual of `k`'s convexity row; 0 without one | the duals |
| `U_k` | the most paths of `k` a solution uses: the upper bound of its convexity row (`row_bounds()`; `max_paths` for a row the master created); infinite without a row or with a `>=`-only row | the master |

- A term is 0 when `b_k - d_k >= 0`, whatever `U_k` (no `inf * 0`).
- `b_k = -inf` (a heuristic stage, a stopped search, a pricer that does not know), or `U_k` infinite with a
  negative `b_k - d_k`: the iteration's `LB` is `-inf`. Bounds after a stopped search: a pricer contract change,
  left to `T-2.1`.
- **The threshold rule:** an exact search that found nothing reports the threshold `d_k - tol` as `b_k`; when a
  pricer returns no column and proves it (the loop's existing `proven` test for that pricer: exact and finished, or
  `NotApplicable`), its term is 0. A
  proven last iteration then gives `LB = z_RMP`, not `z_RMP - U * tol`.
- With `existing_row`, `U_k` assumes the row's other terms are nonnegative (a variable `>= 0` with a coefficient
  `>= 0`), so that the row bounds the pricer's paths; the docstring of `ConvexityRow` says so.
- Valid with artificial columns in the master: the LP with them is below the LP without them.
- The theory, for the docs page: weak duality on any feasible `x` of the full LP gives
  `c x = pi b + sum_p rc_p x_p >= z_RMP + sum_k U_k min(0, rc_min_k)` (the chat's lesson of 2026-11-25).

### 2.2 The stops (`D-311`, `D-312`)

New options of `ColumnGenerationParams` (and the C++ example's params), all off by default:

| option | default | stops when (`LB` finite) | status |
|---|---|---|---|
| `absolute_gap` | 0 (off) | `z_RMP - LB <= absolute_gap` | `GapLimit` |
| `relative_gap` | 0 (off) | `z_RMP - LB <= relative_gap * abs(z_RMP)` (`z_RMP = 0`: only the absolute test) | `GapLimit` |
| `cutoff` | `inf` (off) | `LB >= cutoff - tol` (`ceil(LB - tol) >= cutoff` with `integer_objective`) | `Cutoff` |
| `integer_objective` | `False` | `ceil(LB - tol) >= ceil(z_RMP - tol)` | `GapLimit` |

- `tol`: the run's reduced-cost tolerance (`Tolerances`, the loop's `reduced_cost_tolerance`) (`D-312`, proposed).
- Order in an iteration: solve, price (columns added as today), compute `LB`, then: no new column → today's
  convergence logic (`Optimal`, `NotProven`, `Infeasible`, `on_converged`) wins; new columns → `Cutoff`, then
  `GapLimit`. `on_converged` is not called on these stops.
- After `GapLimit` or `Cutoff`, `objective` is the last LP's value, not the LP optimum; the master holds the last
  columns added (an integer solve uses them).
- `Cutoff`'s use (the chat, 2026-11-25): a known integer solution's cost; `LB >= cutoff` means nothing here beats
  it. Branch-and-price prunes a node so (a direction, `D-275`); at the root it proves a heuristic solution optimal;
  a user with only a threshold stops as soon as the answer is no.
- New statuses after `IterationLimit` in both enums; neither is `Optimal`.

### 2.3 The outputs (`D-313`)

| field | what |
|---|---|
| `ColumnGenerationResult.lower_bound` | the best (highest) `LB` seen over the run; `-inf` when none; `objective` when `Optimal` (as today) |
| `IterationInfo.lower_bound` | the iteration's `LB`, `-inf` when unknown (C++ example: its iteration info the same) |
| the benchmark's `solution.json` | `lower_bound` added; the summary and compare accept the two new statuses |

### 2.4 Branch and commits

Branch `feature/column-generation-lagrangian-bound` from the stack's head (#114, `71c13451`), in `../rcspp-dev`; PR
base #114's branch.

| # | commit | contents |
|---|---|---|
| 1 | `refactor(examples): remove the column generation loop written in Python` (`D-309`) | `examples/column_generation/python/column_generation_loop.py`, its test in `tests/python/test_cpp_column_generation_end_to_end.py` (the run at line 403), `examples/README.md:53`, the example's `README.md` lines 13 and 48 |
| 2 | `feat(column-generation): a Lagrangian bound and early stops on gap or cutoff` | the package: loop, params, result, `IterationInfo`, `ConvexityRow`'s docstring; `docs/advanced/column-generation.md` ("Options and hooks", "How a run ends": the bound, the stops, one paragraph of theory); CHANGELOG; tests |
| 3 | `feat(examples): the Lagrangian bound and early stops in the C++ example layer` | `column_generation.{hpp,cpp}`, bindings, `.pyi` stubs, `cpp_column_generation/params.py`; the example's README; C++ and Python tests |
| 4 | `feat(benchmark): gap, cutoff and integer objective options` | `column_generation.absolute_gap`, `relative_gap`, `cutoff`, `integer_objective` in the three runners and the `cpp-native` program; `solution.json`'s `lower_bound`; summary and compare; `OPTIONS.md`; tests |

### 2.5 Tests (done when: `LB <= z_RMP` at every iteration)

- Package and C++ layer, through `on_iteration`: on C101_25 and R201_25 (exact pricing) `LB <= z_RMP + tol` at every
  iteration, `lower_bound == objective` at `Optimal`, `lower_bound` the maximum of the iterations' bounds.
- `-inf` with a heuristic stage first, without a convexity row (negative reduced cost), and with a `>=` row.
- Several pricers (the toy with two vehicle types): the terms add up, one `-inf` makes the sum `-inf`.
- `existing_row` with an `=` row: `U_k` its right-hand side.
- Each stop triggers on a toy with a known sequence (fake pricers returning set bounds) and gives its status;
  `integer_objective` and `cutoff` with `ceil`; defaults change nothing (iterations and LP equal to today's).
- The three layers: equal `lower_bound` per iteration on C101_25 (the benchmark's layers test, `D-237`'s rule).

## 3. Open questions of the other tasks (each for its own design chat)

| task | ready? | open |
|---|---|---|
| `T-2.5` statistics | ready after `T-2.3` | which candidates ship and each one's switch and cost; `IterationInfo` grows or a record type; the benchmark's per-iteration file (`D-239`) |
| `T-2.1` budget, schedule | no | where the schedule sits (stages inside `GraphPricer` today, the loop's `for` over pricers); the `Pricer` contract; a per-solve stop from Python (missing); a graph per thread (`F-06`); a deterministic mode; a read of #49's `pricing_budget.hpp` (open PR, `feature/pricing-budget`) |
| `T-2.2` pool, removal | no | a column delete in `MasterSolver` (self-test, highspy, gurobipy, the C++ masters) and the column records in step; artificial columns protected (`D-29`); the pool's form: core `SolutionPool` or Python; pricing stored columns is a sparse product: **`D-16` for the user** (numpy, `scipy.sparse`, C++ CSR); its "done when" names the retired baseline file |
| `T-2.4` stabilization | no | after `T-2.3` and `T-2.1`'s proof rule: where the component plugs in (the duals before pricing), mispricing with a heuristic first stage, proof only at the LP's own duals, `LB` at the true duals (`D-25`) |
| all | - | how components are named and registered for the runners; full-word names (`D-12`); state-changing hooks apart from `on_iteration` (`D-31`) |
