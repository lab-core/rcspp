# Phase 1 detailed design (accepted 2026-10-23)

Scope: roadmap T-1.1 to T-1.7. Rows are fixed and no column is removed (`D-34`); the loop is in C++ (`D-37`); Python
users wrap their own model (`D-35`). The `DP-` proposals are resolved (section 7). Names follow #56 (`D-43`). Code
facts checked on `main` @ `129b15e0`.

## 1. Conventions

| topic | rule |
|---|---|
| sense | every backend presents the master as a **minimization**; a backend wrapping a maximizing user model negates costs and duals |
| duals | `π_i = ∂objective/∂rhs_i` of that minimization: `≥` row → `π ≥ 0`, `≤` row → `π ≤ 0`, `=` free (Gurobi `Pi` and HiGHS `row_dual` already follow it); checked by the backend self-test (`D-30`) |
| reduced cost | `c_p − Σ_i a_ip π_i − μ_k` (`μ_k` = dual of pricer `k`'s convexity row) |
| row ids | `0 … num_rows−1`: for the generic master, creation order; for a user-model master, the order of the list the user passes. `Row.index` on arcs uses these ids |
| column ids | `ColumnId` (64-bit), assigned by `MasterProblem`, **stable**; each backend maps ids to its own solver indices, which may shift (all researched solvers renumber on delete) (`D-46`) |
| numbers | `double` everywhere (`#53`); spans for arrays; no `std::map` on hot paths |
| names | `D-12`: `rcspp::column_generation`, full words, `Params` suffix kept |

## 2. C++ API (`cpp/rcspp/column_generation/`, `.hpp` + `.cpp`: `D-56`)

The design as accepted on 2026-10-23, kept for its reasons. **The code differs; the headers are the reference.**
The differences: "As written" at the end of section 6 (PR 1) and the commit table (PRs 2–4).

```cpp
namespace rcspp::column_generation {
using ColumnId = std::uint64_t;
enum class SolverStatus { Optimal, Infeasible, Unbounded, TimeLimit, Other };
struct RowBatch    { std::span<const double> lower, upper; std::span<const std::string> names = {}; };  // D-41
struct ColumnBatch { std::span<const ColumnId> ids; std::span<const double> cost, lower, upper;
                     std::span<const uint32_t> starts, index; std::span<const double> value; };  // CSC
struct Capabilities { bool add_rows, integer_solve, farkas, basis; };

class SolverBackend {                       // one instance = one master LP; virtual calls once per iteration
  public:
    virtual ~SolverBackend() = default;
    virtual size_t num_rows() const = 0;
    virtual void add_rows(const RowBatch&);                   // optional (D-54): throws unless overridden
    virtual void add_columns(const ColumnBatch&) = 0;
    virtual SolverStatus solve_relaxation(const RelaxationParams&) = 0;   // integer columns relaxed in place
    virtual double objective() const = 0;
    virtual void duals(std::span<double> out) const = 0;      // normalized (section 1)
    virtual void primal(std::span<const ColumnId> ids, std::span<double> out) const = 0;
    virtual SolverStatus solve_integer(const IntegerParams&) = 0;         // restores integrality, then MIP
    virtual bool farkas(std::span<double> /*out*/) const { return false; }
    virtual void set_option(std::string_view name, const OptionValue& value) = 0;
    virtual Capabilities capabilities() const = 0;
    // phase 2: remove_columns(std::span<const ColumnId>), set_column_bounds(...)
};
}
```

- `RelaxationParams{time_limit_s, threads, verbose}`; `IntegerParams{time_limit_s, gap, column_type}` with
  `column_type ∈ {Integer (default), Binary}` (`D-51`).
- **Self-test** (`backend_self_test(SolverBackend&)`, also exposed to Python): a 3-row LP with one `=`, one `≥`
  and one `≤` row and known duals; checks the signs, adding columns between solves, `primal` by id, and that a
  maximizing model is normalized. Every backend's test suite runs it.

### MasterProblem

```cpp
class MasterProblem {
  public:
    explicit MasterProblem(SolverBackend& backend);                     // D-38: references
    RowRange add_rows(size_t count, RowSense sense, double rhs);         // generic master only (capabilities)
    size_t add_pricer(Pricer& pricer, ConvexityRow row);                 // returns pricer index k
    void add_columns(size_t pricer, std::span<const Solution> columns);  // initial or generated
    ColumnId add_artificial_column(size_t row, double coefficient, double cost);  // tagged Artificial (D-45)
    ColumnGenerationView view() const;                                   // read-only access for hooks
};
struct ConvexityRow { std::optional<size_t> existing_row; double max_paths = inf; };  // one of the two
```

- `ConvexityRow{.max_paths = U}` creates the row `Σ_{p∈k} x_p ≤ U` (needs `add_rows`); `{.existing_row = i}` uses a
  row the user built (user-model masters). The master adds coefficient 1 in that row to every column of pricer `k`.
- Columns: `Solution.column` as is (`F-01`, `F-02`) plus the convexity coefficient; metadata vector
  `{ColumnId, pricer k, tag (Real | Artificial), Solution}` in step with the backend.

### MasterInitialization (`D-45`)

```cpp
class MasterInitialization {                // how the first LP becomes solvable; swappable (D-17, D-39)
  public:
    virtual ~MasterInitialization() = default;
    virtual void initialize(MasterProblem& master, double cost_scale) = 0;  // once, before the first LP solve
};
class ArtificialColumns final : public MasterInitialization {  // default; registry "artificial_columns"
  public:
    explicit ArtificialColumns(double cost = 0);  // 0 → automatic: 1e6 × cost_scale (max |arc cost|)
};
```

- `ArtificialColumns`: single-phase big-M, one artificial column per linking row, coefficient sign chosen from the
  row's bounds so that it alone satisfies the row. If the converged LP still uses an artificial column above the
  feasibility tolerance, the result is `Infeasible`.
- Later alternatives (phase 2+): `TwoPhaseArtificialColumns` (Coluna phases 0/1/2, `D-26`, needs cost changes in
  the backend), user columns only, Farkas pricing.

### Pricer and strategy

```cpp
enum class PricingStatus { Optimal, SolverLimit, Infeasible, NotApplicable, Unknown };       // D-21
struct PricingRequest { double reduced_cost_threshold; std::optional<double> time_limit_s; };
struct PricingResult  { std::vector<Solution> columns; PricingStatus status;
                        double min_reduced_cost_bound; bool exact_stage; };
struct PricingChanges { bool costs = false, bounds = false, structure = false; };           // D-22

class Pricer {
  public:
    virtual ~Pricer() = default;
    virtual PricingResult price(std::span<const double> duals, const PricingRequest&) = 0;
    virtual void update(const PricingChanges&) {}
};

template <typename... ResourceTypes>
class GraphPricer final : public Pricer {        // refs ResourceGraph<ResourceTypes...> (D-38)
  public:
    GraphPricer(ResourceGraph<ResourceTypes...>& graph, PricingStrategy<ResourceTypes...> strategy,
                size_t cost_index = 0);
};
```

- `GraphPricer::price`: `graph.update_reduced_costs(duals, cost_index)`, then run the strategy's current stage
  with `upper_bound = request.reduced_cost_threshold`. The master passes `threshold = μ_k − tolerance` (convexity
  dual, `api-sketch.md` §6).
- `min_reduced_cost_bound`: the best cost found if the stage is exact and `COMPLETE`, else `−inf` in phase 1
  (a valid bound from a truncated solve comes with the Lagrangian bound work, T-2.3).
- `PricingStrategy<ResourceTypes...>` (`D-23`): ordered stages `{algorithm, AlgorithmParams, exact}`; phase 1 ships
  the stage API (`initial_stage`, `next_stage`, `reset`, `is_exact`) with one exact stage by default;
  heuristics-first and the #49 budget come in T-2.1.

### Tolerances (`D-40`)

```cpp
class Tolerances {
  public:
    virtual ~Tolerances() = default;
    virtual double reduced_cost(const ToleranceContext&) const = 0;  // a column improves if rc < −this
    virtual double feasibility(const ToleranceContext&) const = 0;   // artificial usage, integrality
    virtual double optimality_gap(const ToleranceContext&) const = 0;  // phase 2 early stop (D-28)
};
class AbsoluteTolerances;   // default: 1e-9, 1e-6, 1e-6
class RelativeTolerances;   // scaled by ToleranceContext::cost_scale (max |arc cost| or |LP value|)
```

### ColumnGeneration

```cpp
struct ColumnGenerationParams {
    size_t max_iterations = SIZE_MAX; double time_limit_s = inf;
    std::shared_ptr<const Tolerances> tolerances;                     // null → AbsoluteTolerances
    std::shared_ptr<MasterInitialization> initialization;             // null → ArtificialColumns (D-45)
    StatisticsParams statistics;                                      // tiers 0–1 in phase 1 (D-04, D-42)
    std::function<void(const IterationInfo&)> on_iteration;           // read-only (D-31)
    std::function<ConvergenceAction(ConvergenceContext&)> on_converged;  // D-44
};
enum class ColumnGenerationStatus { Optimal, NotProven, Infeasible, TimeLimit, IterationLimit, SolverError };
struct ColumnGenerationResult { ColumnGenerationStatus status; double lp_value, lower_bound; size_t iterations;
                                Statistics statistics; };

class ColumnGeneration {
  public:
    ColumnGeneration(MasterProblem& master, ColumnGenerationParams params = {});
    ColumnGenerationResult run();
    SolverStatus solve_restricted_master_integer(const IntegerParams&);   // heuristic plan (theory §5.2)
};
```

`run()`, phase 1:

1. `initialization->initialize(master, cost_scale)` (default: the artificial columns).
2. Loop: `solve_relaxation` (not `Optimal` → `SolverError`/`TimeLimit`); `duals`; for each pricer `k`:
   `price(duals, {μ_k − tol})`; add its columns; record statistics; call `on_iteration`.
3. No pricer returned a column: if every pricer is `Optimal` at an exact stage, it is a proven convergence; call
   `on_converged` (if set): `Continue` → back to 2, `Stop` → finish. Otherwise status `NotProven` (`P-05`).
4. Artificial column still used above `feasibility` → `Infeasible`. `lower_bound = lp_value` when proven.

## 3. Python

Redone for `D-60` in `phase-1-python.md` (proposed 2026-10-28), with the research on gurobipy and highspy that was here.

## 4. Build, layout and coverage

CI is deferred (`D-52`): the last column is what the existing gate would measure (`F-14`), not a CI task.

| path | content | coverage |
|---|---|---|
| `cpp/rcspp/column_generation/{types,master_solver,master_solver_support,master_solver_self_test,master_problem,master_initialization,pricer,graph_pricer,pricing_strategy,tolerances,statistics,column_generation}.hpp`, and `.cpp` for the non-template ones (`D-56`) | section 2, as written | 100 % (scripted master solver and pricers in the tests) |
| `cpp/rcspp_backends/highs/highs_master_solver.{hpp,cpp}`, library `rcspp_highs` | `USE_HIGHS`: an installed HiGHS, else `v1.15.1` fetched (`D-82`) | outside the gcovr filter; tested locally |
| `cpp/rcspp_backends/gurobi/gurobi_master_solver.{hpp,cpp}`, library `rcspp_gurobi` | `USE_GUROBI`; `FindGUROBI.cmake` moved from `examples/cpp/vrp/cmake/` to `cmake/` | tested locally |
| `tests/cpp/test_column_generation_*.hpp` (one executable), `tests/cpp/gurobi/`, `tests/cpp/highs/` (one executable each) | section 5 | — |
| Python: bindings, package, tests, `pyproject.toml` | `phase-1-python.md` | `python/bindings/` 100 %, Python 90 % |

## 5. Tests and expected values

| test | setup | expected |
|---|---|---|
| master solver self-test (`D-70`) | 3-row LP (`=`, `≥`, `≤`), then a max version | known duals with the convention of section 1; columns added between solves; `primal` by id |
| toy LP | theory page 1: 3 customers, capacity 11, all 6 routes as columns | LP 37.5; duals (11.5, 12.5, 13.5) |
| toy CG from scratch | same instance as a `ResourceGraph`, no starting columns (artificials) | LP 37.5, status `Optimal`, no artificial column used |
| toy with a fleet limit | the user's row `Σ x_p ≤ 2` / `≤ 1`, passed as `existing_row` (`D-54`) | 37.5, `Optimal` / `Infeasible` (all three customers weigh 15 > 11) |
| initialization swap | custom `MasterInitialization` that adds the 6 toy routes, no artificials | LP 37.5, no `Artificial` column exists |
| convexity threshold | fake backend returning `μ_k = 3` | the pricer receives `threshold = 3 − tolerance` |
| not proven | fake pricer returning `SolverLimit` and no column | status `NotProven` |
| on_converged | hook returning `Continue` once, after changing the fake pricer's answer | the loop resumes, then stops `Optimal` |
| restricted master MIP | toy with all 6 routes as starting columns (from scratch, CG may never generate the single-customer route that the best plan needs) | 44 |
| counterexample (theory §5.2) | 4 customers, **elementary** routes (with cycles allowed, capacity 10 permits revisits and the LP may drop below 40) | LP 40 |
| Python | `phase-1-python.md` §5 | same values as above |
| VRP (`C101_25`, after the RouteOpt stack, T-1.5) | VRP example ported | the T-0.1 golden LP value to 1e-6 |

## 6. PR breakdown (phase 1, from `main`: `D-43`)

| # | PR | contents | needs |
|---|---|---|---|
| 1 | `feat(column_generation): interfaces, master, loop` (#61) | section 2 except backends, `GraphPricer`, `PricingStrategy` and the self-test (with `ArtificialColumns`); scripted backend and pricers; tests of section 5 that need neither | — |
| 2 | `feat(column_generation): Gurobi master solver` (#69) | `GurobiMasterSolver`, `USE_GUROBI`, `FindGUROBI.cmake` moved, the master solver self-test (moved from PR 1: it needs a real LP solver), toy LP | 1 |
| 3 | `feat(column_generation): graph pricer` (#72) | `GraphPricer`, `PricingStrategy` (stages, one exact by default), toy CG end to end | 1, 2 |
| 4 | `feat(column_generation): HiGHS master solver` (#76) | `HighsMasterSolver`, `USE_HIGHS`; the self-test and the end-to-end tests run on it too | 1–3 |
| 5 | `feat(python): column generation bindings` | bindings, trampolines, `_make_pricer`, the C++ HiGHS master solver over a copied model, registry; `phase-1-python.md` | 1–4 |
| 6 | `feat(python): master solvers over the user's model` | `HighspyMasterSolver`, Gurobi (`Q-30`), the wheel with HiGHS, extras, the Python loop (`Q-34`); `phase-1-python.md` | 5 |
| 7 | `docs: column generation` | `docs/advanced/column-generation.md` rewritten, and the pages that show a hand-written loop (T-1.7) | 5, 6 |
| 8 | `refactor(vrp): examples on the column generation layer` | T-1.5 and the Python example; golden baselines (T-0.1) | RouteOpt stack merged |

### Commits per PR

Each commit builds and passes its tests on its own; each adds its headers to the generated `cpp/rcspp/rcspp.hpp`.
Tests: one `tests/cpp/test_column_generation_<part>.hpp` per commit, included by `test_column_generation.cpp`.

| PR | # | commit | contents |
|---|---|---|---|
| 1 | 1.1 | `feat(column_generation): types, master solver and pricer interfaces` | `types.hpp`, `master_solver.hpp` (`solver_backend.hpp` before `D-69`), `pricer.hpp`; test fakes (scripted master solver and pricers); tests of the defaults |
| 1 | 1.2 | `feat(column_generation): swappable tolerances` | `tolerances.hpp` (`D-40`) |
| 1 | 1.3 | `feat(column_generation): the master problem` | `master_problem.hpp`: rows, pricers, convexity rows, CSC column batches (`D-46`, `D-53`) |
| 1 | 1.4 | `feat(column_generation): big-M artificial columns as the default initialization` | `master_initialization.hpp` (`D-45`) |
| 1 | 1.5 | `feat(column_generation): the loop, its statistics and hooks` | `statistics.hpp`, `column_generation.hpp` (`D-44`, `D-31`) |
| 1 | 1.6 | `feat(column_generation): start from the user's model as is` | `NoInitialization`, `RestrictedMasterInfeasible` (`D-57`) |
| 1 | 1.7 | `refactor(column_generation): name the default tolerances and gap` | named constants instead of literals (clang-tidy) |
| 1 | 1.8 | `fix(column_generation): include pricer.hpp where it is used` | an include left in the header after the `.cpp` split |
| 1 | 1.9 | `fix(column_generation): an exact pricer with no path at all proves convergence` | exact `Infeasible` counts as proof, like exact `Optimal` |
| 1 | 1.10 | `test(column_generation): clear the clang-tidy warnings in the tests` | done (`bbde4f28` here, `466be45f` on core): the 48 warnings of the PR 1 test headers; named numbers, designated initializers, hook lambdas moved to helpers. Warnings in code older than this layer (examples, Python bindings, `tests/cpp/util/`) are left alone |
| 2 | 2.1 | `build: FindGUROBI.cmake shared, USE_GUROBI` | done (`c6390e4e`): moved from `examples/cpp/vrp/cmake/` to `cmake/`, the VRP example still finds it; newer library names (`gurobi140`…); off by default (`D-52`) |
| 2 | 2.2 | `feat(column_generation): Gurobi master solver` | done (`21b572ec` here): `GurobiMasterSolver` wrapping a user's `GRBModel` and their row list (`D-54`): columns, LP solve, normalized duals, primal by id, integer solve (`D-61`, `D-62`); fixes the comment at `master_problem.hpp:72`. Primal simplex while `Method` is automatic (`D-65`); Gurobi tests in their own executable, skipped without a license |
| 2 | 2.2b | `fix(column_generation): refuse Gurobi rows that no constraint holds` | done (review): `add_rows` refused ranges only; an infinite equality, `lower = +∞`, `upper = −∞` or NaN now refuse the batch too |
| 1, 2 | 2.2c | (no commit) rename `SolverBackend` to `MasterSolver` | done (`D-67`, `D-69`): every code commit rewritten; hashes above are the new ones. 2.3 uses the new names |
| 2 | 2.3 | `feat(column_generation): master solver self-test` | done (`ec45a43c` here, `D-70`): `master_solver_self_test.{hpp,cpp}`; run on Gurobi, skipped without a license; a scripted master solver making one mistake at a time (run everywhere) shows each is reported; 100 % of the lines covered |
| 2 | 2.3b | `test(column_generation): split the Gurobi refused-rows test` | done (`a5eb6262` here): its clang-tidy complexity warning |
| 2 | 2.4b | `fix(column_generation): a refused Gurobi column batch adds nothing` | done (review of the Gurobi/HiGHS likeness, 2026-10-27; `458f8e15` here, `fe12dcb1` on the Gurobi branch): `add_columns` checked each column as it added it, so a refused batch left its first columns in the model |
| 2 | 2.4c | `refactor(column_generation): what every master solver needs, written once` | done (`0e2d12ce` here, `82aa938d` on the Gurobi branch): `master_solver_support.hpp`, `SolverColumns<SolverColumn>` (ids, a batch checked as a whole) and `row_bounds_can_be_held`; used by Gurobi. The rest of each master solver calls its solver's API and stays its own (my suggestion, accepted: no shared base class) |
| 2 | 2.4d | `fix(build): leave out the solver test folders by file, not by path` | done (`c08ca0ae` here with the HiGHS folder; `c7473b3a` on the Gurobi branch, Gurobi folder only; the HiGHS commit `9973c7d0` adds its folder): the main tests left out every source whose full path held `/gurobi/`, so a checkout in a folder of that name built zero tests (found building a worktree named `gurobi`) |
| 2 | 2.4e | `fix(column_generation): Gurobi relaxes the variables a user adds during a run` | done (review, 2026-10-27; `5070414e` here, `9524d2e9` on the Gurobi branch): user types recorded only at the first LP solve, so an integer variable added later made the LP solves MIP solves (no duals). Now recorded incrementally before every type change; retyping and deleting during a run documented as unsupported |
| 2 | 2.4 | `test(column_generation): the toy on Gurobi` | done (`2f30dfa0` here): through `ColumnGeneration` with the default initialization; the duals as the pricer receives them; route values by id. The toy (described in the test, `D-64`) built as a user would, in a `GRBModel`, then wrapped: LP 37.5 and its duals; restricted master MIP 44 (section 5) |
| 3 | 3.1 | `feat(column_generation): pricing strategy with stages` | done (`2da31017` here, `adc3a335` on `feature/column-generation-pricer`): `pricing_strategy.hpp`; `PricingStage` holds a search function, `PricingStage::make<Algorithm>(params, exact)` (`D-71`); `PricingStrategy` with `next_stage`, `reset`, `is_exact`, one exact stage by default (`D-23`) |
| 3 | 3.2 | `feat(column_generation): graph pricer` | done (`9c7e7040` here, `9194b53c` on the pricer branch): `graph_pricer.hpp`, `GraphPricer` over `ResourceGraph`; stages from the first on every call, stopping at columns or a proof; duals buffer (`D-72`), preprocessing on (`D-75`), proof rule (`D-74`); refuses a graph with no source or sink (its solve returns an empty complete result); `cost_scale` = max \|arc cost\| |
| 3 | 3.2b | `fix(column_generation): state the graph pricer's proof rule once` | done (review; `9025ea87` here, `00ed3f7c` on the pricer branch): the call read `exact && Optimal`, but only an exact finished stage is `Optimal` |
| 3 | 3.2c | `fix(column_generation): the pricing headers in the umbrella header` | done (`a3808598` here, `ce433a59`): 3.1 and 3.2 had not committed the regenerated `rcspp.hpp`. Lesson: stage `cpp/rcspp/rcspp.hpp` after every build that adds a header |
| 3 | 3.3 | `test(column_generation): column generation from scratch` | done (`97755ddc` here, `409f5060`): `tests/cpp/gurobi/test_gurobi_column_generation.cpp`; toy 37.5 `Optimal`, fleet limits 2 (`Optimal`) and 1 (`Infeasible`), four customers 40 with elementary routes (37 without: checked by removing the resource); `routing_graph` in `test_column_generation_graphs.hpp`; Gurobi environment shared (`gurobi_environment.hpp`). Gurobi only until PR 4 |
| 4 | 4.1 | `build: USE_HIGHS, HiGHS installed or fetched` | done (`302d29f6` here, `e6f7eef5` on `feature/column-generation-highs`, a new branch on the pricer one): an installed HiGHS first, else `v1.15.1` fetched and SHA-256 checked (`D-82`); target `highs::highs`; off by default (`D-52`). Checked both ways: fetched (builds in about a minute) and an install given by `highs_DIR`. The `rcspp_highs` library comes with 4.2 |
| 4 | 4.2 | `feat(column_generation): HiGHS master solver` | done (`5f9eca40` here, `9973c7d0` on the HiGHS branch; 14 tests in `tests-rcspp-highs`, self-test included; `D-78` checked: duals valid after `solve_relaxation` on an all-integer model; time limit per solve `F-25`; threads `D-83`). Planned: `HighsMasterSolver` wrapping a user's `Highs` and their row indices (`D-54`, `D-77`): CSC columns, LP solve with `solve_relaxation` after checking it gives duals (`D-78`), normalized duals, primal by id, integer solve; simplex method: HiGHS's default, left alone, already takes primal simplex after added columns and is the fastest (`D-79`, `F-24`); infeasible or unbounded re-solved without presolve (`D-80`); a warning when the thread count changes (`D-81`, as built `D-83`); basis reset and retry on a false cycling stop (`F-21`) |
| 4 | 4.2b | `refactor(column_generation): the HiGHS master solver uses the shared rules` | done (`3798c7ac` here, `275097f5` on the HiGHS branch): `SolverColumns<HighsInt>`, `row_bounds_can_be_held` |
| 4 | 4.2c | `docs(column_generation): which upper bounds the HiGHS master solver puts back` | done (`c65854b0` here, `b7de61b7`): the bounds of the columns as added; a user's change during the run is undone after a binary integer solve |
| 4 | 4.3 | `test(column_generation): the end-to-end tests on every master solver` | done (`d19f46c2` here, `16e302ff` on the HiGHS branch): `tests/cpp/test_column_generation_end_to_end.hpp`, typed tests (toy with every route: LP, duals, values, MIP 44; from scratch: toy, fleet limits 2 and 1, four customers 40) instantiated by each solver's executable with a small user model type (`GurobiUserModel`, `HighsUserModel`); the Gurobi toy and from-scratch tests moved there. All pass on HiGHS as they are. Full run: 662 + 1 skipped, Gurobi 21, HiGHS 19 |
| all | R.0 | (no commit) `num_rows`, `num_columns`, `num_pricers` → `number_of_*` | done (`D-98`, 2026-10-28): the plan branch and the four PR branches rewritten by one rename script (`git filter-branch`), every commit kept building; every hash in this table is the rewritten one |
| 1 | R.1 | review round of 2026-10-28 (`D-93`–`D-99`), 8 commits | done (here / on core): `1ec72808`/`fb05d7c1` time limit reached in pricing → `TimeLimit`; `a65fba95`/`432915b3` a row listed twice summed, one warning; `ea918a70`/`03ef0bc0` a master initialized once whatever the loop; `91a9eda3`/`3c7ed5fb` `MasterSolver::restore_user_model`, called when `run` ends; `8d182e5b`/`e0cdba8e` `MasterSolver::dual_tolerance` bounds the reduced-cost tolerance; `ce29eaa4`/`26325b37` `largest_user_cost` in the scale, artificial cost in the statistics, `Infeasible` docs; `e450407d`/`9be52fea` `NotApplicable` does not block the proof; `dd33ff8e`/`5ce1d6c1` interface docs |
| 2 | R.2 | review round, 10 commits | done (here / on Gurobi): `1379a9f3`/`4948de96` self-test integer step on a fractional LP (0.55 vs 1.0); `29c01770`/`6b8987db` shared check refuses non-finite numbers; `631e0a34`/`cee36aae` Gurobi options back after each solve, types at the end (pending until Gurobi's next update, so values stay readable); `125fb5c5`/`98b9ffc0` relaxed binary within [0, 1] (`F-28`); `13c5b043`/`6aeec3e8` re-solve gets the time left; `75e9344a`/`392bd076` `OptimalityTol`, user costs; `3da001de`/`c3c1da87` refused option → `std::invalid_argument`; `ee4a45bf`/`6755e38e` `FindGUROBI.cmake` hints only, `NAMES_PER_DIR`, include path; `35ee6348`/`78368900` Gurobi tests on `rcspp_objects`; `175acfb2`/`1c29b50f` `OneEntryColumns` |
| 3 | R.3 | review round, 2 commits | done (here / on pricer): `d91e26f4`/`ad07a4bb` an out-of-range cost index throws; `644b5133`/`3fd7b82f` the cursor's docs |
| 4 | R.4 | review round, 12 commits | done (here / on HiGHS): `55db1062`/`feebf14c` a refused `addCols` throws, nothing recorded; `7a0e1ce5`/`501332c8` clocks zeroed before each solve (`F-29`); `6bf36270`/`93f68561` options back after each solve, binary bounds at the end; `84f8ab67`/`0f00bea9` thread retry only after a refused run; `51c25660`/`6ce1e9e6` binary solve keeps a tightened bound; `8763dcff`/`ac8bfd53` integer option range; `35074744`/`778b5e4a` no solution read without one; `a96a6a35`/`8467ac0a` `dual_feasibility_tolerance`, user costs; `6ea81571`/`3076939e` include path; `c8eed0d8`/`7686e956` HiGHS tests on `rcspp_objects`; `5c021886`/`8d02103c` end-to-end tests in their namespace; `6c73c02c`/`9bc883b3` `OneEntryColumns`. Full run after: main 671 + 1 skipped, Gurobi 24, HiGHS 24 |
| 5, 6 | — | Python commits | `phase-1-python.md` §6 |
| 7 | 7.1 | `docs: column generation` | `docs/advanced/column-generation.md` (T-1.7); the hand-written loops of `docs/cpp/quickstart.md` and `docs/python/quickstart.md` point to the layer; `docs/index.md`'s feature list; the Python API page (`docs/python/api.rst`) and package tree (`docs/python/index.md`); the HiGHS master solver's choices (time limit per solve, threads, simplex left to HiGHS); includes the Gurobi LP method (`D-65`): primal simplex while `Method` is automatic, why, the measurement (`F-19`), how to choose another |
| 8 | 8.1 | `refactor(vrp): C++ example and benchmark on the layer` | T-1.5 |
| 8 | 8.2 | `refactor(vrp): Python example on a user-model master` | |
| 8 | 8.3 | `docs(plans): golden baselines` | T-0.1 |

### PR 1 as written: changes from section 2

| change | why |
|---|---|
| `SolverBackend::row_bounds(lower, upper)` | `ArtificialColumns` needs the row bounds; a wrapped model already holds them |
| `Pricer::cost_scale()` (default 1) | automatic big-M cost and relative tolerances need a scale before any column exists |
| `ColumnBatch::{starts, row_indices, values}` | clearer than `index` / `value` |
| `MasterProblem::add_artificial_columns(rows, coefficients, cost)` | one backend call for all artificial columns |
| `MasterInitialization::initialize(master, ToleranceContext)` | room for more than the cost scale |
| artificial column on every row that `x = 0` violates | also covers an equality convexity row |
| `timeout_s`; `ColumnGenerationResult::objective` | the name `AlgorithmParams` uses; no "lp" abbreviation |
| an infeasible restricted master gives `SolverError` | it proves nothing without Farkas pricing (phase 2) |
| `SolverBackend::add_rows` optional, throws by default (`D-54`) | a wrapped model's rows exist; only backends that can add rows override it |
| no plan IDs in code comments (`D-55`) | the plans are removed at the end |
| `.cpp` for `tolerances`, `master_problem`, `master_initialization`, `column_generation` (`D-56`) | non-template code |
| `NoInitialization` and `RestrictedMasterInfeasible` (`D-57`) | a feasible user model needs no artificial columns |
| `Unbounded` status; the solver-status mapping lists every value | an unbounded master is a modeling problem, and a proof |
| an exact pricer reporting `Infeasible` proves convergence | no path of any cost means no improving path |

## 7. Design proposals (resolved 2026-10-23)

| ID | decision | ID | decision |
|---|---|---|---|
| DP-01 | `D-45` (big-M behind `MasterInitialization`) | DP-05 | `D-49` |
| DP-02 | `D-46` | DP-06 | `D-50` |
| DP-03 | `D-47` | DP-07 | `D-51` |
| DP-04 | `D-48` | | |
