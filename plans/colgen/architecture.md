# Architecture (proposal: status follows `decisions.md`)

## Layers

```text
 user code (C++ or Python)
   │ builds master rows + ResourceGraph(s) whose arcs carry Row{index}
   ▼
 rcspp::column_generation  (NEW, header-only, solver-agnostic)
   BranchAndPrice ──► ColumnGeneration ──► MasterProblem ──► SolverBackend ◄── HiGHS | Gurobi | SoPlex | fake(tests)
        │                  │   │                                          (NEW, separate targets: D-07)
        │                  │   ├─► DualStabilization
        │                  │   ├─► ColumnPool  = SolutionPool               (EXISTS P-08)
        │                  │   └─► Pricer[k] ─► PricingStrategy ─► ResourceGraph::update_reduced_costs / solve
        │                  │                                                (EXISTS P-03, P-04)
        │                  └─► Statistics / hooks
        └─► BranchingRule ─► ResourceGraph::remove_arcs / restore_arcs, pool filters   (EXISTS P-16)
```

Alternative at the top level (phase 4, `D-32`): the **SCIP pricer adapter**. SCIP owns the B&P tree and its LP;
a SCIP pricer plugin (`ObjPricer` in C++, `Pricer` in PySCIPOpt) calls our `Pricer` with SCIP's duals, adds the
columns (`SCIPaddPricedVar`) and reports the bound (`*lowerbound`). Branching = an arc-branching rule + a marker
constraint handler driving `remove_arcs`/`restore_arcs` (SCIP Binpacking pattern). It reuses `Pricer`,
`PricingStrategy`, `DualStabilization` (smoothing form), the pool and `Statistics` (`D-24`), not `ColumnGeneration`,
`MasterProblem` or `SolverBackend`. Settings: separators, restarts and presolve off (prior-art.md).

## Glue between existing and new code

The key link: **master row `i` = `Row.index` `i` = `duals[i]`**.

1. The user declares master rows (`sense`, `rhs`) and gets their indices.
2. The user builds graphs with `add_arc(…, rows={Row{i, a}})`, as today.
3. Pricing returns `Solution`s whose `column` holds the original cost and aggregated rows (`F-01`, `F-02`).
4. `MasterProblem::add_columns(solutions)` passes `column` to the backend as CSR. No domain code involved.
5. Convexity rows (one per pricer `k`, "at most `U_k` paths") are rows on the arcs leaving the source, or are
   added by the master to each column of pricer `k` (`Q-03`).

## User-model masters (Python, `D-35`)

```python
model = gp.Model()                                    # the user's own model, any extra rows or variables
cover = [model.addConstr(... == 1) for i in customers]
fleet = model.addConstr(... <= n_vehicles)
master = GurobipyMaster(model, rows=cover)            # graph Row{i} <-> cover[i]
master.add_pricer(pricer, convexity_row=fleet)
result = ColumnGeneration(master, ...).run()
```

- One wrapper per library (`GurobipyMaster`, `HighspyMaster`, `PyscipoptMaster`, `PythonMipMaster`), all with the
  same small contract: add a batch of columns to existing constraints, solve the relaxation, return objective,
  primal values and **normalized** duals, switch to integer for the final solve. Each passes the backend self-test.
- Rows are fixed (`D-34`): the wrapper never adds rows; it only adds columns (and the artificial columns of phase 0,
  `D-26`, as variables).
- Integer variables of the user's model are relaxed during CG and restored afterwards.
- Columns are added in batches (Python cost per column is small next to the LP solve, `D-36`).
- Under option A (`Q-24`) a wrapper is a Python `SolverBackend` subclass called by the C++ loop; under option B it is
  plain Python used by the Python loop. The contract is the same either way.

## Change list by component

`N` new · `A` adapted (existing code changes) · `U` unchanged. Mirrors the summaries' tables (README links).

| component | | change | refs |
|---|---|---|---|
| `SolverBackend` | N | rows, columns, LP solve, primal + duals, column bounds/removal, optional MIP | D-19 |
| C++ backends `HighsBackend`, `GurobiBackend`, `SoplexBackend` | N | HiGHS default (in the wheel, pinned), Gurobi `USE_GUROBI`, SoPlex `USE_SOPLEX`; outside `cpp/rcspp/`; each passes the backend self-test | D-07, D-14, D-30, D-32 |
| Python backends | N | subclasses over `gurobipy`, `highspy`, user code; `python-mip` low priority | D-14, D-32 |
| User-model masters (Python) | N | wrap the user's gurobipy/highspy/pyscipopt/python-mip model: rows fixed, columns added in batches, duals normalized | D-34, D-35 |
| SCIP pricer adapter | N | optional target: rcspp as SCIP's pricer + arc-branching constraint handler (C++ and PySCIPOpt) | D-24, D-32 |
| `MasterProblem` | N | `=`/`≥`/`≤` rows (ranges inside), convexity row per pricer, phases 0/1/2 with artificial columns, column metadata + cleanup, consumes `Solution.column` | D-13, D-26, D-29 |
| `Pricer` / `GraphPricer` | N | wraps one `ResourceGraph`; threshold shifted by its convexity dual; returns status + bound; `update(changes)` | D-21, D-22 |
| `PricingStrategy` | N | stages heuristic → exact (`next_stage`, `reset`, `is_exact`); budget escalation from #49; multi-pricer options | D-23, D-27 |
| `DualStabilization` | N | none (default), smoothing with automatic α + mispricing schedule; box-step later | D-25 |
| `ColumnGeneration` | N | loop, stopping rules, Lagrangian bound, gap-based early stop, result | D-28 |
| `Statistics` + hooks | N | tiers 0–3, standard log columns, read-only iteration hooks | D-04, D-31 |
| configuration registry | N | named components for benchmark grids | Q-17 |
| `BranchAndPrice` | N | design only; alternative route = SCIP pricer adapter | D-20, D-32 |
| `SolutionPool` / `PricingPool` | A | used by the loop: re-pricing, activity, CSR export | P-08, F-04 |
| `Row`, `Solution` | A | `coefficient` → `double`; docstring fix | I-01, I-02 |
| Python bindings | A | `rcspp.column_generation` module, trampolines | D-08 |
| VRP examples (C++, Python) | A | rewritten on the layer; example masters removed | T-1.5, T-1.6 |
| benchmarks | A | BKS table, machine-readable output, layer statistics | I-03, I-05 |
| build and CI | A | `USE_HIGHS`/`USE_GUROBI`…; CI (backend coverage rules, benchmark workflow without Gurobi) deferred | Q-05, I-07, D-52 |
| logger | A | no ANSI colors outside a terminal | I-04 |
| docs | A | `docs/advanced/column-generation.md` rewritten; theory page 5 points to the layer | T-1.7 |
| `ResourceGraph`, resources, labeling algorithms, preprocessing | U | used through `update_reduced_costs`, `solve`, `remove_arcs` | P-03, P-04, P-16 |

## One CG iteration

| step | call | existing / new |
|---|---|---|
| 1 | `backend.solve_linear_relaxation()` (warm-started, columns added incrementally) | NEW |
| 2 | `stabilization.pricing_duals(lp_duals)` | NEW (generalizes `P-12`) |
| 3 | `pool.price(duals, threshold)`: re-check stored columns first (cheap) | EXISTS `SolutionPool::price` |
| 4 | for each `k`: `pricer[k].price(duals, request)` → strategy → `update_reduced_costs` + `solve` | wraps `P-03`, `P-04`; strategy from `P-06` / #49 |
| 5 | `pool.add(sols)`, `master.add_columns(sols)` | EXISTS + NEW |
| 6 | Lagrangian bound from each pricer's bound (`D-21`; formula even when smoothing, `D-25`); stop on gap (`D-28`), `LB ≥ cutoff`, or (no column ∧ every pricer `Optimal` at an exact stage) | NEW (`P-13`); proof rule from `P-05` |
| 7 | `statistics.record(iteration)`; `hooks.on_iteration(info)` (null-checked) | NEW |

## Interfaces (sketch, C++)

```cpp
namespace rcspp::column_generation {
// Rows are ranges lower <= A x <= upper (=: lower == upper). Duals: one convention for every backend,
// pi = d(objective)/d(rhs) of a minimization (>= row: pi >= 0, <= row: pi <= 0); backend self-test (D-30).
struct RowBatch    { std::span<const double> lower, upper; };
struct ColumnBatch { std::span<const double> cost, lower, upper;
                     std::span<const uint32_t> starts, index; std::span<const double> value; };  // CSC (D-30)
struct LinearRelaxationResult { SolverStatus status; double objective; std::span<const double> primal, duals; };

class SolverBackend {                                   // one virtual call per iteration: overhead is negligible
  public:
    virtual ~SolverBackend() = default;
    virtual size_t add_rows(const RowBatch&) = 0;                 // returns first row id
    virtual size_t add_columns(const ColumnBatch&) = 0;           // returns first column id
    virtual void   set_column_bounds(std::span<const size_t> ids, double lower, double upper) = 0;
    virtual void   remove_columns(std::span<const size_t> sorted_ids) = 0;  // ids compact afterwards (D-29)
    virtual LinearRelaxationResult solve_linear_relaxation(const LinearRelaxationParams&) = 0;  // warm start implicit
    virtual IntegerSolveResult     solve_integer(const IntegerSolveParams&) = 0;  // optional: capabilities()
    virtual void set_option(std::string_view name, OptionValue value) = 0;        // beyond the typed params
    virtual Capabilities capabilities() const = 0;   // integer, basis get/set, Farkas, LP-method hints
};

enum class PricingStatus { Optimal, SolverLimit, Infeasible, NotApplicable, Unknown };   // D-21
struct PricingRequest { double reduced_cost_threshold = -1e-9; std::optional<double> time_s; };
struct PricingResult  { std::vector<Solution> columns; PricingStatus status;
                        double min_reduced_cost_bound;   // lower bound on this pricer's best reduced cost
                        bool exact_stage; };             // proof = status == Optimal && exact_stage
struct PricingChanges { bool costs, bounds, structure; };                                // D-22

class Pricer {                                      // type-erased (F-03); one per subproblem k;
  public:                                           // callable outside our loop, e.g. from a SCIP pricer (D-24)
    virtual ~Pricer() = default;
    virtual PricingResult price(std::span<const double> duals, const PricingRequest&) = 0;
    virtual void update(const PricingChanges&) {}
};
template <typename... R> class GraphPricer final : public Pricer;  // refs ResourceGraph<R...> + PricingStrategy
}
```

- `PricingStrategy` (`D-23`): ordered stages `(algorithm, params)`, heuristic first, exact last.
  `initial_stage()`, `next_stage()` when a stage finds nothing, `reset()` after a success, `is_exact()`. Budget
  escalation of #49 inside the exact stage. Multi-pricer options (`D-27`): max columns per round, pricer order by past
  success, pricers per round, all pricers every N rounds.
- `DualStabilization` (`D-25`): `None` (default), `Smoothing` (automatic α, mispricing schedule: on a misprice, lower
  α and re-price; α = 0 is plain pricing). Interface: `pricing_duals(lp_duals)`, `on_pricing(result)`,
  `is_mispricing(...)`. The Lagrangian bound is always computed from its formula with the pricers' bounds.
- `MasterProblem` (`D-26` → `D-45`: phase 1 ships big-M only, behind `MasterInitialization`; `D-29`): phase 0
  (artificial columns, big costs), phase 1 (real columns at zero cost), phase 2 (normal); a column-metadata vector
  in step with the backend's indices; tags protect artificial and stabilization columns from cleanup; cleanup
  threshold + ratio; duplicate prevention.
- `ColumnGeneration<Params>`: owns the loop. Early stop (`D-28`): absolute/relative gap, integral-objective rounding.
  `run()` returns `ColumnGenerationResult{status, lp_value, lower_bound, proven, iterations, statistics}`.

## Performance rules

1. Nothing new runs per label. CG code works per iteration, where an LP solve and a pricing solve cost ms to s.
2. Dense `std::vector`/`span` indexed by row and column id. No `std::map` (the examples use it, `P-09`).
3. Incremental LP: add columns in batches, never rebuild; keep the basis (warm start).
4. Move `Solution`s; pass columns to backends as CSR spans.
5. Python callbacks (trampolines, hooks) only at per-iteration granularity, never inside pricing.
6. Every perf-sensitive change is measured against `T-0.1` baselines.

## Statistics and diagnostics: tiers

| tier | content | cost | toggle |
|---|---|---|---|
| 0 | final `ColumnGenerationResult` (status, values, iterations) | ~0 | always on |
| 1 | counters + per-phase time (LP, pricing per stage, pool, stabilization) | ~1 clock read per phase per iteration | runtime `params.statistics.timers` (default on) |
| 2 | per-iteration records, standard columns (`D-31`: iteration, time, LP time, pricing time, #columns, LP value, Lagrangian bound, incumbent) + duals norm, min reduced cost | one push per iteration | runtime `params.statistics.trace` (default off) |
| 3 | checks: column feasibility, dual signs vs senses, recomputed reduced costs | O(columns) per iteration | runtime `params.debug_checks` (off); compiled out with `NDEBUG`? (`Q-06`) |
| L | instrumentation inside labeling (`P-14`, #41) | per label | **compile-time** only (template policy or macro) |

- Hooks: `std::function<void(const IterationInfo&)>`, read-only (`D-31`). An empty function costs one branch per
  iteration. State-changing extension points (e.g. adding cuts) are a separate interface.
- Benchmark output: tier 1 and 2 as a flat record (JSON lines or CSV), with a schema per run config (`Q-07`).

## Data interchange: numpy / scipy.sparse (`Q-13`)

Every sparse object in CG has the same shape: a matrix in CSR/CSC format.

| object | rows × cols | used for | already |
|---|---|---|---|
| master matrix `A` | master rows × columns (one column = one path, few nonzeros) | LP backends; all solver APIs take CSC/CSR arrays | `SolutionPool::get_lp_data` exports CSR (`P-08`) |
| pool pricing `c − Aᵀπ` | sparse mat-vec over stored columns | re-pricing stored columns | `PricingPool` numpy fast path |
| arc-flow `f = B x` (`B` = arc × column incidence) | sparse mat-vec | arc branching (T-3.2) | — |
| arc → row coefficients | arcs × master rows | building the graph | `add_rows(np.array)` (`P-02`) |

Rules:

- C++: CSR in `std::vector`s, passed as `std::span`.
- Python boundary: numpy arrays, zero-copy where possible (pybind11 buffer protocol), e.g. duals in, primal and
  duals out, columns as `(starts, index, value)`.
- `scipy.sparse`: optional. Accepted as input and offered as output (`as_scipy()`), never a required dependency (`F-10`).
- `scipy.optimize.linprog`/`milp` run HiGHS internally but **can't warm-start or add columns**: they rebuild the LP
  every call. Fine as a test-only reference backend (cross-checks LP values and duals), not for performance.

## Code layout and build (proposal)

| path | content | coverage gate |
|---|---|---|
| `cpp/rcspp/column_generation/` | interfaces, `MasterProblem`, `ColumnGeneration`, `GraphPricer`, stabilization, statistics | 100 % (tested with a fake backend + HiGHS) |
| `cpp/rcspp_backends/highs/` | `HighsBackend`, CMake `USE_HIGHS` (find_package, else FetchContent pinned to a tag) | in CI (free) |
| `cpp/rcspp_backends/gurobi/` | `GurobiBackend`, `USE_GUROBI` | excluded, or tested locally only (`Q-05`) |
| `cpp/rcspp_backends/soplex/` | `SoplexBackend`, `USE_SOPLEX` | in CI (Apache-2.0) |
| `cpp/rcspp_scip/` | SCIP pricer adapter + arc-branching constraint handler, `USE_SCIP` (phase 4) | in CI (Apache-2.0) |
| `python/bindings/column_generation.cpp` | binds `rcspp::column_generation`; `SolverBackend` and `Pricer` trampolines (subclassable from Python) | 100 % |
| `python/src/rcspp/column_generation/` | Python API; backends over `gurobipy` / `highspy` (`D-14`); PySCIPOpt pricer example (phase 4) | 90 % |
