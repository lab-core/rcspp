# Prior art (researched 2026-10-21)

Every fact below was taken from a page the researchers opened. URLs are in "Sources". `unverified` = not confirmed on
an opened page. Proposed plan changes are the `PA-` items; all accepted on 2026-10-21 as
`D-21`…`D-33` (`decisions.md`).

## Projects at a glance

| project | what | license | language | status (2026-10) | role for us |
|---|---|---|---|---|---|
| SCIP 10.1.0 | branch-cut-and-price framework; bundles SoPlex | Apache-2.0 (since 8.0.3) | C | release 2026-09-18 | optional host of B&P (rcspp as its pricer) |
| SoPlex 8.1.0 | simplex LP solver of SCIP; exact rational LP | Apache-2.0 | C++ | release 2026-09-18 with SCIP 10.1.0 | possible LP backend (instead of SCIP itself) |
| PySCIPOpt 6.3.0 | Python API of SCIP; wheels bundle SCIP | MIT | Python/Cython | 2026-10-11, CPython 3.9–3.15 | Python route to SCIP pricers |
| GCG 4.1.0 | generic Dantzig-Wolfe B&P on SCIP; PyGCGOpt 0.4.0 (2024-12, looks stale) | Apache-2.0 (checked 2026-10-26); optional Cliquer and GSL are GPL-3.0 | C/C++ | in SCIP Opt Suite 10.1.0; 5.0 notes in the repo | reference design (pricing loop, stabilization) |
| Coluna.jl 0.8.2 | B&P framework of swappable algorithms | MPL-2.0 | Julia | release 2025-07, push 2026-09 | reference design (stages, phases, API) |
| BaPCod / VRPSolver | state-of-the-art VRP BCP | academic, **not open source** | C++ | — | reference only |
| RouteOpt 2.0 | exact CVRP/VRPTW BCP | **GPL-3.0** (Gitee "academic use" unverified) | C++ | commits 2026-10-13 | ideas only, no code reuse (we are MIT) |
| vrpy + cspy | Python VRP CG (PuLP master, cspy RCSPP) | MIT | Python / C++ | vrpy unmaintained since 2023-04 | cautionary example |
| HiGHS 1.15.1 | LP/MIP solver | MIT | C++ | 2026-07-02 | default backend (`D-07`) |
| python-mip 2.0.0 | MIP modelling, CBC + HiGHS | EPL-2.0 | Python | 2026-08-18, ~1 maintainer, CBC crash reports | low-priority backend |

## Comparison with our components

| our component | GCG | Coluna.jl | RouteOpt | vrpy |
|---|---|---|---|---|
| `SolverBackend` | SCIP only | any MOI solver | Gurobi/CPLEX/MindOpt, compile-time macro, Gurobi-like wrapper | PuLP |
| artificial columns | off by default, big-M 1000; Farkas pricing | phases 0/1/2, costs 1e6 global / 1e4 local | ±1 columns per row (stabilization) | none |
| convexity rows | per block, its dual passed to the solver | lower/upper multiplicity per subproblem | — | per vehicle type |
| `Pricer` contract | `solve` + `solveHeuristic` + `update(objs, bounds, conss changed)`; status `Optimal/Infeasible/Unbounded/SolverLimit/NotApplicable/Unknown`; returns a **lower bound** | callback submits columns **and a dual bound** | modes LIGHT/HEAVY/EXACT | internal |
| `PricingStrategy` | solvers by priority; heuristic iterations; pricer sorting; chunk size; eager frequency 10 | stages heuristic → exact (`next_stage`, `is_exact_stage`) | LIGHT → HEAVY → EXACT with time limit, "complete" flag | BestEdges1/2, BestPaths, Hyper, then exact |
| `DualStabilization` | smoothing + mispricing schedule (+ hybrid ascent); on, root only | self-adjusting smoothing α, off by default | du Merle box penalties | none |
| column pool / cleanup | pool, age limit 100, duplicate prevention on | cleanup threshold 10 000, ratio 0.66 | quantile-based deletion, reduced-cost fixing, tags protect artificial columns | none |
| Lagrangian bound, early stop | `abortpricinggap`, `abortpricingint` | `compute_dual_bound`, abs/rel tolerances | `lp + (n/avg route length)·min_rc` | none (stagnation counter 1000) |
| statistics / hooks | SCIP statistics | printer columns `it, et, mst, sp, cols, al, DB, mlp, PB`; before/after-iteration hooks (printing only) | every N iterations: LP and pricing time, LP value, UB | counters |
| selection by name | SCIP parameter strings | Julia parameter structs | macros | strings |
| B&P | Vanderbeck, original-variable, Ryan-Foster, strong branching | depth-first default, strong branching (beta) | strong branching (bkf), bbt | dive only |

## SCIP: the colleague's question

- **SCIP inside rcspp (as an LP solver): no.** SCIP's FAQ advises against using SCIP as a pure LP solver (presolve,
  no reduced costs, no Farkas proof). Use **SoPlex's C++ API directly** (`addColsReal`, `removeColReal`, `getDualReal`,
  `getBasis`/`setBasis`) if we want that solver: a `SoplexBackend`, about the size of the HiGHS one. SCIP's LP interface
  (LPI) wraps many solvers, but its HiGHS interface is "beta, sometimes failing".
- **rcspp inside SCIP (as its pricer): yes, as an optional adapter.** SCIP owns the tree, LP warm starts and
  heuristics. We would write:
  - a pricer (`ObjPricer` in C++, `Pricer` in PySCIPOpt) mapping SCIP's duals to `update_reduced_costs`, running
    rcspp, adding columns with `SCIPaddPricedVar`, and returning a Lagrangian bound in `*lowerbound`;
  - a Farkas pricing path (when the LP is infeasible);
  - an arc-branching rule + a marker constraint handler that applies and undoes our `remove_arcs` per node (pattern:
    SCIP's Binpacking example, `branch_ryanfoster.c` + `cons_samediff.c`); it must also handle pseudo solutions.
  - Settings: separators, restarts and presolve off; constraints marked "modifiable".
  - Limits: SCIP is mostly sequential; no built-in stabilization in SCIP itself (its FAQ: objective coefficients
    cannot change, so do weighted-sum smoothing in the pricer; GCG has a stabilization class); a column can only be
    removed at the node that created it, and column deletion is beta.
- **Our own loop stays the foundation** (`D-06`, `D-19`). The SCIP route reuses our pricer, pricing strategy,
  stabilization (smoothing form), pool and statistics, but not our master or `SolverBackend`.

## SCIP pros and cons (colleague question, 2026-10-26)

Colleague page: <https://claude.ai/artifact/4pQWNzgaSNZXiKpwoWUZp8>. One-liner: not for column generation alone; a
serious option for B&P with rcspp as SCIP's pricer.

| use | pros | cons |
|---|---|---|
| CG only (root LP) | pricer plugin: reduced-cost and Farkas pricing, `lowerbound`, `stopearly`; LPI swappable (SoPlex, Gurobi ≥ 7.0.2, CPLEX, Xpress, CLP, Glop, Mosek, QSopt, HiGHS beta); PySCIPOpt 6.3.0 wheels bundle SCIP | presolve + transformed problem on top of the LP (FAQ: call an LP solver directly); nothing over our own backends; our master and backends not reused |
| B&P (SCIP owns the tree) | tree, node LP warm starts, primal heuristics on the master; custom branching via constraint handlers (Binpacking: `branch_ryanfoster` + `cons_samediff`); arc branching = branching on master arc variables (VRP example) or our own rule; Python pricers and branching rules; GCG adds Dantzig–Wolfe, branching rules, stabilization, column pool, `solver_xyz` pricing-solver template | mostly sequential (parallelism via TPI concurrent solves or UG only); must switch off separators, the dual-fixing presolver, possibly `propagating/rootredcost/freq=-1`; rows `modifiable`; objective fixed during the solve (no box-step); column deletion beta, only at the creating node; large API, harder debugging (original vs transformed problem) |

Published use: SCIP's `examples/VRP` (C++ pricer); arXiv 2401.17937 (cycle partition: PySCIPOpt B&P, Rust parallel
pricer, "the rest of SCIP is sequential"); arXiv 2602.08673 (EVRPTW branch-price-and-cut on GCG 3.7.1 / SCIP 9.2.1,
Gurobi 11 as LP solver, own labeling).

## SoPlex vs HiGHS for the master LP (colleague question, 2026-10-26)

Colleague page: <https://claude.ai/artifact/8XFo11Zp2BeTHQ21vXLABw>. Benchmarks: `F-22`. One-liner: HiGHS about 8×
faster, MIT, pip wheels; SoPlex's edge is exact rational LP, so HiGHS stays the default (`D-07`).

| | HiGHS 1.15.1 | SoPlex 8.1.0 |
|---|---|---|
| CG API | `addCols`, `deleteCols` (range, set, mask), `getBasis`/`setBasis`, `getSolution` (duals), `getDualRay` | `addColsReal`, `removeColsReal`, `getBasis`/`setBasis`, `getDualReal`, `getDualFarkasReal` |
| warm start | new columns nonbasic, no refactorization; bug `F-21` | "hot-start from any regular basis"; no CG reports found |
| exact LP | no | rational LU, iterative refinement, optional quad precision |
| parallelism | serial dual simplex by default; parallel IPM (HiPO); GPU PDLP ("not robust") | sequential |
| Python | `highspy` (MIT, wheels) | none on PyPI (C and Rust interfaces) |
| build | CMake, FetchContent options (`FAST_BUILD`) | CMake, C++14; GMP, MPFR, PaPILO on by default; `BOOST=OFF` for the library only (inference) |

Other open-source LP solvers: CLP 1.17.11 (EPL-2.0, mature simplex, warm start); cuOpt 26.10 (Apache-2.0, GPU,
first-order: no simplex warm start); OR-Tools PDLP and Glop (Apache-2.0); cuPDLPx, HPR-LP (GPU research codes,
approximate duals). None beats HiGHS for a warm-started CG master on what is public.

## Proposed changes to the plan (`PA-`)

| ID | proposal | from | affects |
|---|---|---|---|
| PA-01 | `Pricer::price` returns a **status** (`Optimal`, `SolverLimit`, `Infeasible`, `NotApplicable`, `Unknown`) and a **lower bound on its minimum reduced cost**, not just `exact: bool`. Only `Optimal` from an exact stage proves; the bound gives a Lagrangian bound even after a truncated solve | GCG, Coluna, #49 | architecture.md `Pricer`, T-1.4, T-2.3 |
| PA-02 | `Pricer` gets an `update(changes)` call (costs, bounds, structure changed) so it can keep cached state between iterations and later carry branching decisions | GCG | T-1.4, phase 3 |
| PA-03 | `PricingStrategy` = explicit **stages**: `initial_stage`, `next_stage` (nothing found → go more exact), `reset` (after success → back to cheap), `is_exact` | Coluna, RouteOpt | T-2.1 |
| PA-04 | Pricer, strategy, stabilization and statistics are callable **without our loop** (duals in → columns + bound out), so a SCIP pricer can reuse them | SCIP research | T-1.1 design |
| PA-05 | Stabilization default alternative = **smoothing with an automatic α and a mispricing schedule**; the Lagrangian bound is computed from its formula, never from the smoothed LP value. Box-step (du Merle) = later alternative | GCG, Coluna, RouteOpt | T-2.4 |
| PA-06 | Initial feasibility (`Q-16`): **phases 0/1/2 with artificial columns** (configurable costs) as default; Farkas pricing as alternative | Coluna, SCIP, GCG | T-1.1, T-3.1 |
| PA-07 | Multi-pricer loop options: max columns per round, pricer ordering (by past success), how many pricers per round, "all pricers every N rounds" | GCG | T-2.x |
| PA-08 | Early stop options: absolute/relative gap between Lagrangian bound and LP; "integral objective" rounding | GCG, Coluna | T-2.3 |
| PA-09 | Column management: a column-metadata vector kept in step with the LP (indices compact on delete); tags protecting artificial and stabilization columns; cleanup threshold + ratio; duplicate prevention on by default; pool checked before pricing | RouteOpt, Coluna, GCG | T-2.2 |
| PA-10 | `SolverBackend`: rows as ranges `lo ≤ Ax ≤ up`; columns in CSC batches; **one documented dual sign convention + a backend self-test** on a 2-row LP; warm start implicit; optional basis get/set; an LP-method hint (primal after adding columns, dual after adding rows); typed common options + `set_option(name, value)` | HiGHS, Gurobi, RouteOpt | T-1.1, T-1.2 |
| PA-11 | Standard per-iteration log columns (iteration, time, LP time, pricing time, #columns, LP value, Lagrangian bound, incumbent); iteration hooks are read-only; state-changing hooks are a separate interface | Coluna, RouteOpt | T-2.5 |
| PA-12 | Backend list: HiGHS (default, FetchContent pinned to v1.15.1), Gurobi, **SoPlex instead of "SCIP backend"**; python-mip = low priority (it now wraps highspy; CBC crash reports); SCIP = the optional pricer adapter above | all | D-07, D-14, Q-21, Q-22 |
| PA-13 | No code from RouteOpt (GPL-3.0) or VRPSolver (closed); ideas only. GCG license to check before reading its code closely | licenses | process |

## Sources

SCIP/SoPlex/PySCIPOpt: <https://github.com/scipopt/scip/releases>, <https://raw.githubusercontent.com/scipopt/scip/master/doc/xternal.c>,
<https://raw.githubusercontent.com/scipopt/scip/master/doc/inc/faq/faqtext.txt>,
<https://raw.githubusercontent.com/scipopt/scip/master/examples/Binpacking/src/pricer_binpacking.c>,
<https://raw.githubusercontent.com/scipopt/scip/master/examples/VRP/src/pricer_vrp.cpp>,
<https://raw.githubusercontent.com/scipopt/scip/master/src/lpi/lpi.h>, <https://raw.githubusercontent.com/scipopt/soplex/master/src/soplex.h>,
<https://pypi.org/pypi/PySCIPOpt/6.3.0/json>, <https://pyscipopt.readthedocs.io/en/latest/install.html>,
<https://raw.githubusercontent.com/scipopt/PySCIPOpt/master/tests/test_pricer.py>.
GCG: <https://gcg.or.rwth-aachen.de/>, <https://gcg.or.rwth-aachen.de/doc/pricing-params.html>,
<https://raw.githubusercontent.com/scipopt/gcg/master/src/gcg/type_solver.h>, <https://github.com/scipopt/gcg>,
<https://scipopt.github.io/PyGCGOpt/api/pricingsolver.html>.
Coluna: <https://atoptima.github.io/Coluna.jl/stable/api/colgen/>,
<https://raw.githubusercontent.com/atoptima/Coluna.jl/master/src/Algorithm/colgen.jl>,
<https://atoptima.github.io/Coluna.jl/stable/start/pricing/>.
VRPSolver: <https://vrpsolver.math.u-bordeaux.fr/>, <https://optimization-online.org/2019/06/7238/>.
RouteOpt: <https://github.com/Zhengzhong-You/RouteOpt>, <https://zhengzhong-you.github.io/RouteOpt-Docs/common/solver.html>.
vrpy/cspy: <https://vrpy.readthedocs.io/en/latest/solving_options.html>, <https://github.com/torressa/cspy>.
HiGHS: <https://github.com/ERGO-Code/HiGHS/releases>, <https://raw.githubusercontent.com/ERGO-Code/HiGHS/master/highs/Highs.h>,
<https://ergo-code.github.io/HiGHS/dev/guide/further/>, <https://ergo-code.github.io/HiGHS/dev/terminology/>, <https://pypi.org/project/highspy/>.
Gurobi: <https://docs.gurobi.com/projects/optimizer/en/current/reference/attributes/constraintlinear.html>.
python-mip: <https://pypi.org/project/mip/>, <https://raw.githubusercontent.com/coin-or/python-mip/master/CHANGELOG.md>,
<https://github.com/coin-or/python-mip/issues>. 2026-10-26: <https://www.scipopt.org/doc/html/FAQ.php>,
<https://www.scipopt.org/doc/html/PRICER.php>, <https://www.scipopt.org/doc/html/LPI.php>,
<https://raw.githubusercontent.com/scipopt/PySCIPOpt/master/src/pyscipopt/pricer.pxi>,
<https://raw.githubusercontent.com/scipopt/gcg/master/README.md>, <https://soplex.zib.de/>,
<https://plato.asu.edu/ftp/lpopt.html>, <https://plato.asu.edu/ftp/network.html>, <https://arxiv.org/html/2401.17937>,
<https://arxiv.org/html/2602.08673>, <https://github.com/ERGO-Code/HiGHS/issues/1785>. SciPy: <https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.linprog.html>.
