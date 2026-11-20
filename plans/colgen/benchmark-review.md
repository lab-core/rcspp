# Benchmark review: PR #97 read before it leaves draft (`T-1.10`)

Read only, 2026-11-19 (Opus; four subagents: options + docs, runners + instances, `benchmark.py` + CI, summary +
compare; the blockers re-checked by hand). Head `ac832b35`, base #88 `0597e768`, 41 files, +10,150 lines.

## 1. Checks run

| check | result |
|---|---|
| `pytest benchmarks/column_generation/tests`, `RCSPP_BENCHMARK_BUILD=build-python-highs` | 286 passed, 0 skipped, 67 s |
| same, no variable (six builds in `../rcspp-dev`) | 213 errors "several builds": documented, but a fresh reader hits it first |
| coverage, in process (subprocess runs not counted) | 98 %; untouched: `runner.gurobipy_model`, package gurobipy master, `record.processor_name` non-Windows, `collectors` Linux peak memory |
| pre-commit on the PR's files, pyright strict | clean |
| ASCII in code, `D-55` words in code, docs, commits | clean (one `×` in `OPTIONS.md:266`, Markdown) |
| CI | ci-full needs the `run-ci` label: not run on #97; Linux and macOS process code never ran |

## 2. Blockers (wrong results or lost data)

| id | where | what | fix |
|---|---|---|---|
| R4 | `run_folder.py:105-116` | `--overwrite` runs `rmtree` on any folder holding `metadata.json`: a runner pointed at a benchmark folder deletes the whole benchmark (re-checked) | remove only `RUN_FILES` and the log; refuse a folder with `benchmark.toml` or `runs/` |
| B1 | `benchmark.py:650-669`, `:767-785` | the resume counts any folder with `result.json` as finished, whatever its recorded outcome (crashed after writing, interrupted): never re-run, missing from the figures, exit 0 (re-checked) | re-run folders whose outcome is crashed / not started / interrupted; `interrupt()` finishes a run that already wrote its result |
| B2 | `options/benchmark.py` `unique_names` | names that differ only in case (`[master.Fast]`, `[master.fast]`) share one folder on Windows / macOS: one run's options and results overwrite the other's, "2 / 2 finished" | clash on `casefold()`, add the fingerprint or refuse |
| S1 | `compare.py:342-371` `side_figures` | the time rule refuses only on timeouts: A crashed in 2 of 3 rounds still gets `equivalent`, exit 0 | refuse when any round is not finished, as for timeouts |
| R2 | `package_runner.py:67-75` | gurobipy parameters go to `setParam` directly: `Method = "0"` crashes with `TypeError` (exit 1, "crashed"); the cpp runner accepts it through `set_option` | use `solver.set_option` inside `except ValueError`, as the cpp runner does |
| R1 | `record.py` `version_record` | the HiGHS version is read from `highspy` if imported: `cpp-bindings --master highs` in its own process records none; `--master highspy` on cpp-bindings records the wheel's HiGHS, not the linked one that solves; the test passes only because pytest imported highspy | record the linked HiGHS (bindings attribute or `CMakeCache`'s version), highspy's apart; test in a subprocess. Overlaps `F-70` (3) / `T-1.19` |
| R3 | `runner.py:93-98`, both runners' `read_solution` | a run whose last LP failed (`TimeLimit` inside the LP, `SolverError`, infeasible) reads that unfinished LP's values under the previous objective; `solve_lp_again` has no time limit | trust `read_solution` only after a finished LP, else solve again or `lp_missing`; give the re-solve a time limit; fix the comment and README |
| O2 | `options/run.py:114-116` `name_part` | a named `[integer.off]` never names the run (the run's block went through `for_instance`, the definition did not): `...-integer_4af6c6` | compare the definition after `for_instance` |
| O4 | `options/blocks.py:121-137` | `tolerances = "relative"` in `[defaults]` plus a dotted tolerance in a run fails; `OPTIONS.md:219` says the name equals the table | turn a name into `{type = name}` before merging |
| B3 | `benchmark.py:707-708`, `load.tree_processor_seconds` | each 50 ms poll lists the process tree: on Windows ~60 % of a core with 4 runs, counted in every run's `other_processor_percent` | read processor time only at a sample, or cache the tree after start |
| B5, B10 | `benchmark.py:163-169`, `:767-791` | SIGTERM / SIGHUP (cron, `timeout`, closed terminal) or a second Ctrl-C during `interrupt()` leave runners running as orphans in their own sessions | handle SIGTERM / SIGHUP as Ctrl-C; ignore SIGINT inside `interrupt()`, kill all trees then wait |
| B4 | `benchmark.py:386-428` | no lock: a second benchmark on the same folder deletes the first's runs in progress | `output/.lock` with PID + start time, taken over when stale |

## 3. Should fix

| id | where | what | fix |
|---|---|---|---|
| O1 | `options/run.py:213`, `options/benchmark.py:128, :199` | errors from a file are labelled "command line" | `complete(entry.where or self.defaults.where)`; `over` joins both places |
| O3 | `options/benchmark.py:77, :249`, `options/run.py:171` | `benchmark = 1`, `layer = ["package"]`, `from = 1` raise `TypeError` (traceback, not exit 2) | type checks raising `OptionsError` |
| O5 | `options/benchmark.py:122` | a run's name depends on the benchmark file's `[defaults]`: the runner names the same options differently, two benchmarks can name different runs alike; `compare.py` pairs by name | name against the presets' defaults (as `expand_run`) |
| O6 | `options/benchmark.py:126-150` | every matrix point dropped: 0 runs, exit 0; the docs say a missing master "is refused" | refuse "no run left"; reword `OPTIONS.md:162` |
| O8 | `options/blocks.py:271-293, :327` | negative `max_iterations` / `timeout_s`, nan `relative_gap` accepted | `not x >= 0` checks |
| O9 | `source_tree.py:102-107` | `is_inside` raises `ValueError` across drives (installed rcspp on `D:`) | catch and return False |
| O10 | `source_tree.py:53-65` | an absolute `--build` of another checkout passes; its `_core` is measured under this tree's commit; relative `--build` is from the tree root, undocumented | refuse a build outside the tree; say "relative to the tree's root" |
| O7 | `options/command_line.py:35` | `--stage "tabu:ids=[0,26]"` splits inside the list | split on commas outside brackets, or document |
| B6 | `record.py` `source_record` | a dirty tree changed again and rebuilt resumes silently (`uncommitted_changes` is a bool); README:165 says it is refused | hash of `git diff HEAD` (and `_core`'s size / mtime) |
| B7 | `benchmark.py:689-692, :737-742` | STOP during the last runs: exit 3, no `finished` event, though all ended; hides runs failed | stop only when runs are waiting |
| B8, B9 | `benchmark.py:340, :410-459` | `--processes` change reported as "another source, versions or machine"; a threads change told to resume with the same processes | drop `benchmark` from `differences()`; branch only when processes differ |
| B11, B13 | `benchmark.py:794-822`, `:550-558` | a runner started but not yet in `running` escapes `interrupt()`; `psutil.Process` of a fast-exited child raises on Windows (benchmark FAILED); a reaped PID killed on Linux | append the Popen first, catch `NoSuchProcess`; kill only `poll() is None` |
| B12 | `benchmark.py:142-144, :645-685` | the scan and `summarize` are outside the `try`: a bad `result.json` gives a traceback and exit 1 (= runs failed) | widen the `try`, return FAILED |
| B14 | `benchmark.py:689` | a STOP file left from before stops the next start at once, undocumented | remove it in `interrupt()`, or say so at start |
| B24 | `benchmark.py:803-810` | runner stdout to a file is block-buffered: a run killed at its timeout leaves an empty log | `PYTHONUNBUFFERED=1` or `-u` |
| R5 | both runners, `runner.py:296` | HiGHS keeps its first solve's threads: `integer.threads = 4` runs with the LP's, `run.toml` says 4 | refuse a different integer threads for HiGHS, or document; record it |
| R6 | `record.py:372` `code_record` | `build_type` null with Visual Studio: Debug and Release `_core` indistinguishable | bindings expose their configuration (`NDEBUG`) |
| R7 | `runner.py:108-110` | an `ERRORS` exception after `start_folder` gives exit 2 ("not started") on a half-written folder; `gurobipy_model`'s `ValueError` a traceback | exit 1 with a note after the folder starts; README: a stopped run needs `--overwrite` |
| S2 | `compare.py` (script) `percent`, `seconds` | `nan` / `inf` accepted: always `slower`, always `equivalent`, floor dropped | `math.isfinite` |
| S3 | `summary.py:233-246` `group` | rounds of one name with different `run.toml` merged under the first's options | flag "options differ between rounds" |
| S4, S5 | `summary.py:342-366, :775-815` | CSV columns ordered by first appearance (fields of later runs after `flags`); a result field named `run`, `rounds`, `outcome`... overwrites the fixed column | fixed column groups; refuse or prefix colliding names |
| R12 | both runners' docstrings, `cpp_bindings_runner.py:72` | "parameters can turn the log on" is false (refused); a dead `output_flag` line | drop both |

## 4. Test gaps (beyond the fixes above, each fix with its test)

- Unit: `relative_gap` zero branches, `integer_results` with no integer solution, `gurobipy_model` rows, `load`
  (process gone, clamping), `instances.resolve_solution` rounding, `Benchmark.from_table` bad types, `stage()` lists
  and spaces, master refusals beyond threads (`R10`, `O17`); compare: B side's reasons, B median 0, `.` and
  `folder:` sides, CSV disagreement strings (`S19`); summary: interrupted / not-started outcomes, CSV quoting,
  rounds beyond `rounds` (`S20`); script: child processes killed at timeout, STOP cases, Ctrl-C with two runs,
  `other_runs` branches, `summarize` failing (`B17`).
- Integration:
  - gurobipy never runs in a runner test, on either layer (`R11`); add a `master` parametrization, skipped without a
    licence;
  - `benchmark.py` never starts the `cpp-bindings` runner; `example.toml` only expanded, never `--check build`
    (`B16`);
  - no `column_generation.timeout_s`, `integer.timeout_s`, `integer.threads` run (`R11`);
  - summary and compare never read a folder made by the real runners: one header line is checked, the fixtures lack
    `record`, `code`, `start` (`S21`); add summary and compare asserts to `test_every_kind_of_input_runs_and_runs_again`;
  - the runner's name of a benchmark run never compared with the benchmark's (`O5`, `O18`).
- CI (`B15`): the `build` fixture skips everything when no build is found, so the job can go green empty; set
  `RCSPP_BENCHMARK_BUILD: build` and `-rs` in `ci-full.yml`; run ci-full on #97 before it leaves draft.

## 5. Docs and readability

| id | what | fix |
|---|---|---|
| O14, O19 | `OPTIONS.md`: deeper dotted names only one level on the command line; `threads` default 1 when processes > 1; name rule; README: `_core` not checked against the C++ sources | add the caveats |
| B18, B20, B22, B23 | `benchmark.py --help` lacks exit codes and the timeout-kept rule; `cut_off` also counts crashed folders; `example.toml` does not say `cpp-bindings` and `highs` need the example built | add them; rename `cut_off` to `unfinished` |
| S11, S12, S13 | `--time-within 0.05` is 0.05 %; compare's CSV `kind`, `judgement` and range strings; summary's measure suffixes; "Rounds not finished" lists timeouts | document; rename the section "Rounds not in the figures" |
| R14 | each runner hands five callables to `runner.main`: the order of the steps is not in the file meant to teach the layer | each runner its own short `main` calling the steps in order; `runner.py` keeps folder and record helpers |
| R15 | `describe_columns`, `read_solution`, `solve_lp_again`, `solve_integer` copied between runners | keep (teaching code), a comment naming the twin, a test of equal outputs |
| S14 | `summary.py` (835 lines) holds the reader compare imports 20 names from; two table writers | the reader in `folder.py`; one table function |
| B25, B26, B27 | `carry_out` 113 lines; `Opening` a mutated helper class; nested helpers in `other_runs` | split into scan / loop / end; plain tuple instead of `Opening` |
| nits | `O11`–`O13`, `O15`, `O16`, `S6`–`S10`, `S15`–`S18`, `B19`, `B21`, `B28`–`B30`, `R8`, `R9`, `R13`, `R16`, `R17` | in the subagents' reports, summarised in the chat of 2026-11-19; fix with the commit they touch |

## 6. Decided (2026-11-19)

`D-227`–`D-232`. Commits on top of `ac832b35`, in this order, each with its tests (failing test first where it fits):

| # | commit | findings |
|---|---|---|
| 1 | options: done `1f823b59` (29 tests added, 315 pass) | O1–O8, O12–O16, B2, O5 naming (O11 with 2) |
| 2 | source tree: done `06ff47f7` (7 tests added, 322 pass) | O9, O10 (`CMakeCache`), O11, O19 |
| 3 | runners: done `35f42019` (24 tests added, 346 pass) | R14 (own `main`), R2, R3, R4, R5, R7–R9, R12, R13, R15–R17; limits and gurobipy tests |
| 4 | `benchmark.py`: done `f96247b8` (20 tests added, 366 pass) | B1 (`--retry-crashes`), R4's resume warning, B3–B14, B21, B24–B27; `cpp-bindings` and `example.toml --check build` tests |
| 5 | summary and compare: done `310b4b22` (24 tests added, 390 pass) | S14 (reader module), S1–S13, S15–S18; the integration test on a real folder |
| 6 | docs and CI: done `324fac42` (390 pass) | B15, B18–B23, O14, O19, S11–S13 wording left, `ci-full.yml` |

R1 and R6 to `T-1.19`. Then run-ci on #97, then out of draft.

Left open: B17 (l), `check_options`' "instance type has no start" branch, unreachable with the shipped type; the
Linux and macOS signal paths untested locally (Docker Desktop off), first run in ci-full.

## 7. ci-full after the fixes (2026-11-20, `F-74`)

| failure | where | cause | fix |
|---|---|---|---|
| `test_the_command_line_is_written_quoted` | Ubuntu, macOS | the test looked for Windows's quoting; the code is right | `b6edcb20`: split back with `shlex` on POSIX |
| `test_every_layer_gives_the_same_run` | Ubuntu only | one `cpp-bindings` run: 25 iterations against 16, objective equal; HiGHS 1.15.1 everywhere | not our code (`F-75`): HiGHS builds return duals 1e-13 apart on a degenerate LP, the pricer keeps other tied paths; `8b80353b` compares the objective across layers, iterations within one HiGHS build (`D-237`); `bd6822d4` the time rule says when iterations differ (`D-238`) |

The Linux and macOS signal paths left open in §6 pass.

## 8. A long run on Linux (2026-11-20, `F-77`)

`layers-50` on the home server (`T-1.24`, `home-server-runs.md`), at `b6edcb20`:

| finding | fix |
|---|---|
| a round killed at its timeout (or running) has no record; the summary counted it as a second, empty one: "mixed", "solver version not recorded" | `ee8b0272`: `folder.has_record`, rounds without a record left out of the records and the solver check; a test |
| `requirements.txt` left `highspy` unpinned (1.16.0) while the example builds HiGHS 1.15.1 | `ee8b0272`: `highspy==1.15.1`, with why |
| the README's build command lacked `-DCMAKE_CXX_CLANG_TIDY=`; it suggested the example's Gurobi option, which the benchmark never uses | `ee8b0272`: both lines |
| a run prints nothing while it runs: a timeout leaves no progress | not here: `D-239` (per-iteration data written as it happens), `D-240` |
