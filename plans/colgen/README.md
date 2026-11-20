# Column generation layer: plan

Status (2026-11-11): **Python layer only (`D-116`).** A pure-Python column generation layer is built, then compared
with the C++ layer (`D-111`, `T-1.15`); whether the C++ layer stays is open (`Q-41`). Until then the C++ layer is
frozen as it is: stacked drafts #61, #69, #72, #76, the Python PRs 5–6 and packaging #80. Big-M start (`T-1.13`)
done: fix A + B (`D-118`) reaches `Optimal` up to R201 and ships with `T-1.15`; the big-M start stays 3.9 times
slower with exact pricing on R201, tabu first cuts it 5.3 times (`F-41`).
Heuristic pricing is done (`T-1.14`, `F-40`; `D-117`: no default cap, tabu optional).
**Next:** `T-1.15`, the pure-Python layer (`python-layer.md`): on top of the stack, the C++ layer's features written the
Python way (`D-132`, superseding `D-121`); design `D-119`–`D-126`, pool postponed. Steps A–F done: commits 1–6
(`4881e169`, `0e620a91`, `ad46b14d`, `2ed8bbd5`, `4c5e1269`, `6c899842`), reviews B, D, F and the Python-way review
folded in (`D-127`–`D-136`); copied to `feature/column-generation-pure-python` (pushed, on #80's head `8bb6f36c`;
the same six patches as here, new hashes `6f9e8d6a`…`510e9b65`).
Step G done 2026-11-06 (`python-layer-runs.md`): LP values, iterations and columns equal on C101_25, R201_25 and
RC201_12 over highspy and gurobipy (and C101_50, C201 over highspy); times within 0–30 ms of the C++-backed layer
on the small instances (C201: +4.7 %, noisy machine); probes of B.15, B.16, D.12 and the `ColumnBatch` copy with
proposals. Decided (`D-137`, `D-135` confirmed): B.15, B.16, D.12 unchanged; `ColumnBatch` and `RowBatch` copy
their inputs (folded into commit 1; hashes after it). Records: `python-layer-records.md`,
`python-layer-runs.md`. Step H done 2026-11-06 (`python-layer-comparison.md`): the pure layer adds 1–2.4 ms per
CG iteration, spread over `add_columns`, the master solver, the graph pricer and the loop (`F-47`); LP values equal
with tabu too; capped greedy does not finish on R2 instances (`F-48`). **Proposed, for the user:** the matrix
(`D-138`: 4 configurations × exact/tabu × single/big-M, three tiers, about 1.5 h plus 6 h overnight on the idle
desktop), `Q-41`'s rule (`D-139`: within max(5 %, 0.25 s) per cell; the floor is the user's call), D.12 unchanged
(`D-140`). Full matrix run 2026-11-07/08 on the desktop (`D-138` accepted for running;
`python-layer-comparison.md` §6–7, `F-49`): 1,204 runs, LP, iterations and columns equal everywhere, 108 / 108 cells
equivalent by `D-139` (also with a 0.1 s floor). **Proposed:** `D-141`, Python only. **Next:** the user decides
`D-139`–`D-141` (`D-141` with the colleagues; presentation 2026-11-09, slides done 2026-11-09 in
`benchmark-results/colgen/presentation/python-layer/`: 9 slides + 3 backup, no plan IDs, the same-API slide, a
chart of the runs over 100 s against their run-to-run spread), then the last PR (`python-layer.md` §2). PR
description for the six commits drafted 2026-11-09 (base #80; the user opens the PR and reviews it there; the review
comments go to a later chat). PR #88 review done 2026-11-09 (R.1–R.22 in `python-layer-records.md`: two HiGHS
Ctrl-C / interrupt bugs, tests, the user's comments, a readability pass), approved and folded into commits 1–6 on
both branches (hashes in the record); worktree `../rcspp-dev` on `feature/column-generation-pure-python`. Both
branches force-pushed 2026-11-09.
Colleagues' meeting 2026-11-09: Python only ships, the C++ layer and its bindings stay as an isolated example
(`D-142`, amends `D-141`); the VRP examples ported to the layers after a speed check (`D-143`, supersedes `D-122`).
Feedback, analysis, inventory of what moves and open questions: `separation.md` (`Q-50`–`Q-55`, `T-1.17`–`T-1.19`).
Design chat 2026-11-10: `D-139`–`D-143` accepted; `D-144` (the example's bindings compiled into `_core` behind an
off-by-default option, sources under `examples/`, inert hooks in core), `D-145` (wheel without HiGHS), `D-146`
(example on in CI, out of the coverage gates), `D-147` (stack merged at once, PRs kept with paths rewritten, pure
layer its own PR), `D-148` (VRP rule), `D-149` (what the layers lack, e.g. pools, stays), `D-150` (one folder
`examples/column_generation/`, top-level package `cpp_column_generation`, C++ names kept); `D-151` (option names
`USE_COLUMN_GENERATION_EXAMPLE{,_GUROBI,_HIGHS}`). `D-144` prototype done 2026-11-10 (`separation.md` §7, `F-50`;
local throwaway branch `prototype/column-generation-example`, `a2ed4f5d`): only inert hooks left in core; MSVC off (no
column generation symbols) and on (C++ and Python tests pass, Ctrl-C stops the example's pricer) pass; Linux gcc 11
off passes, on needs gcc >= 13 (`<format>`). The pure layer needs from the stack only `graph.py`'s helpers (#77) and
numpy, extras, `highspy` test requirement (#78). Prototype review fixed 2026-11-10 (`separation.md` §7): five
commits `eb5ca442`…`325bba34` on the prototype branch (local, not pushed); Linux gcc 13 on (Docker: C++ 697, core
Python 544, example 89 with both SIGINT tests), MSVC on (C++ 729, core 579, example 116) and off (C++ 602, core
579, example skips, no column generation symbols in `_core`) pass. Isort moves the example tests' path-setting
import after the package's unless `# isort: split` holds it. Stack rebased onto `main` `6367ee56` (#56–#59)
2026-11-11, first step of `T-1.17`'s rewrite (`separation.md` §8): one branch per PR, local only (not pushed),
backups `backup/colgen-<name>-before-main-rebase`, worktree `../rcspp-rebase`; 7 conflicts kept both sides; main's
renames applied in the commits that brought the old names; every head passes on MSVC, #88 on Linux gcc 13.
`F-53` decided 2026-11-12 (`D-152`, `D-153`); the eight branches force-pushed by the user (checked equal to `origin`
2026-11-12). Fold map written 2026-11-12 (`fold-map.md`, `fold-map-commits.md`, `fold-map-targets.md`; filter
`tools/fold_filter.py`, dry run `F-54`, traps `F-55`): 90 commits → 89, `D-154`–`D-158` accepted 2026-11-12,
`Q-56`–`Q-59` decided (`D-159`–`D-162`). Rewrite done 2026-11-12 (`fold-record.md`): 89 commits, every head passes on
MSVC off and on with §8's totals, pre-commit clean, final state the 16 intended differences; departures and `F-57`
recorded; Linux gcc 13 passes at #77, #80, #88 (`fold-record.md` §5); `Q-60` decided (`D-166`, a #88 fixup); the eight
branches moved and force-pushed 2026-11-13, PR descriptions updated. **Next:** `T-1.18`; `T-1.19`
after the layout (also the benchmark's pure C++ leg, `D-164`); `drivers.py`
after the push; then `T-1.20` (many cheap iterations over three legs, `F-56`, `D-163`), a colleague's question of
2026-11-12. Triage (`D-165`): `T-1.7` and a read of each folded PR before the stack leaves draft, then the merge; then
`T-1.18`–`T-1.21` and `T-1.16` as follow-up PRs; then re-scoping `T-1.9`, `T-1.10` and phases 2–4.
2026-11-13: run-ci passed on #88's head. The user's order: `drivers.py` (Sonnet) and the read of the folded PRs
(Opus) now, then `T-1.7`, then a benchmark design chat (`T-1.10` earlier, grown by each follow-up task); the comment
on #60 just before the merge; the merge waits. The plan branch still tracks the pre-fold stack: `drivers.py` runs the
folded code from a checkout of #88 (`../rcspp-dev`).
Read of the folded PRs done 2026-11-13 (`fold-review.md`, `F-58`): heads as expected; 45 findings, 2 blockers (no
CHANGELOG entries; #80's install test leaks into FetchContent consumers), 18 should fix, 25 nits; **Next:** the user
answered 2026-11-13 (`D-167`–`D-170`). Fixes done (`fold-review.md` §5): one commit per PR on local `fix/*`
(worktree `../rcspp-fix`), HiGHS bound bug fixed with a test, every head passes MSVC off and on, pre-commit clean;
branches force-pushed and titles and bodies applied 2026-11-13. **Next:** the user runs run-ci on #88; then `T-1.7`.
2026-11-13: drivers adapted (commit `feat(benchmark): run the drivers on the folded stack from a source checkout`):
`COLGEN_SOURCE_DIRECTORY`, default `../rcspp-dev`; every layer configuration on C101_25 equals the recorded runs
(`benchmark-results/colgen/post-fold-check*/`, `comparison.md`); `AlgorithmParams.stop_after_X_solutions` is
`max_solutions` now; `example-mip` is not deterministic (LP equal).
Order since 2026-11-13 (`D-171`): **Next:** `T-1.7`, a commit in #88 with no default master solver (`D-172`,
`D-173`; run-ci on #88's fixes running); then `T-1.10`'s runner, grown by `T-1.18`, `T-1.19`, `T-1.16`, `T-1.21`,
`T-1.20` in that order; then the re-scoping chat (`T-1.9`, phases 2–4); the comment on #60 just before the merge; the
merge waits for the user. The runner's questions (where it lives, the master solver benchmark apart, what each run
records, statistics, `T-1.9`) wait for `T-1.10`'s design chat. Stale questions closed 2026-11-13.
`T-1.7` done 2026-11-13 (three commits on #88, pushed as a fast-forward to `0597e768`: the page, the tabu fix, the
page's test; `D-174`–`D-176`); **Next:** run-ci on #88, then `T-1.10`'s design chat; `F-60`: a default tabu stage
does not run, no autodoc of the package with the docs' mocks. `../rcspp-dev` is still at `f31257f0` (before the fold
review's fixes): move it to #88's head before the drivers measure anything.
Local docs preview with `docs/theory` (`F-61`): sources in `../rcspp-docs-preview` (server stopped; rebuild and
serve as `F-61` says).
2026-11-16 (the laptop): run-ci passed on #88's head `0597e768` (2026-11-15). `../rcspp-dev` moved there from the
local `prototype/column-generation-example` (kept); the old `f31257f0` kept as `backup/colgen-pure-python-f31257f0`,
`../rcspp-rebase` detached; `_core` built in `../rcspp-dev/build-python-highs`, imports and source guard checked.
`T-1.10`'s design done (`benchmark-runner.md`, `D-177`–`D-187`): runners per layer, a benchmark of processes, TOML
blocks, a folder per run; master solver benchmark set aside (`D-179`).
`T-1.10` steps 1–4 done 2026-11-16 (`F-62`): branch `feature/column-generation-benchmark` from #88 `0597e768`, in
`../rcspp-dev` (`../rcspp-benchmark` is a stale detached worktree of the first version); `tomli-w` in `.venv`. For steps
5–9: the runners parse `--file`, `--output`, `--build`, `--machine` themselves and the rest with
`options.parse_assignments`; `[benchmark]` takes whole numbers only (step 7 adds `timeout`, `machine`, `logs`); check
every run's instance file before the first run; the artificial cost is `cost_factor` × max(1, `cost_scale()`), passed
to both layers; gurobipy's free row as ≥ −1e30; the README gains its runner, benchmark and output sections with steps
5, 7, 8. Options code and docs decided (`D-195`, `D-196`, `D-188` accepted); `D-190`–`D-196` and the rename folded
into steps 1–4, force-pushed 2026-11-16: `58a9071b`, `6c3b19cc`, `67822d53`, `f1aff5e7` (old head kept as local
`backup/benchmark-steps-1-4-8e944acd`; 75 tests). 2026-11-17: pyright strict clean (`76c4f444`, not pushed); the
options as dataclasses and the full `OPTIONS.md` (`D-197`–`D-202`, `589966ad`, on top, pushed; 119 tests).
Step 5 done 2026-11-17 (`F-63`): the `package` runner, `61cf282c` on top, not pushed (133 tests).
Solutions designed 2026-11-17 (`benchmark-solutions.md`, `D-203`–`D-206`, `Q-61`): `--overwrite` and `solution.json`
as step 5's fixup; the integer solve and the instance's resolved, checked solution after step 6; per-iteration data
with `T-1.20`. Step 5's fixup done and folded 2026-11-17 (`F-64`, `D-207`, `D-208`): the runner is `374d2ea6`
(replaces `61cf282c`, not pushed; 140 tests); `solution.json` has no LP after a limit hit after pricing.
Step 6 done 2026-11-17 (`F-65`): the `cpp-bindings` runner `f97e6f73` on top (pushed; 170 tests, pyright and
pre-commit clean); every layer equal on C101_25 (16 / 599, 23 / 691, 18 / 666, 29 / 723; `highs` = `highspy`).
Then `D-209` (`column_generation.threads`; a master's own thread parameter never reached a solve) and `D-210`
(runners in `runners/`): `36c9490b`, `4ddc513e` on top (`F-66`, 177 tests); branch pushed 2026-11-17 (head
`4ddc513e`). Integer solve done 2026-11-17 (`benchmark-solutions.md` §2, `F-67`): `094beacd`, then `7190eb83`
(the runners' shared steps in `runner.main`); pushed 2026-11-17, head `7190eb83` (206 tests); the LP solved again
after a limit (`D-211`), the gap (`D-212`), defaults and a failed check (`D-213`); R201_25's integer master is
17.8 % (gurobipy) to 20.3 % (HiGHS) above the LP.
Step 7 done 2026-11-17 (`F-68`, `D-214`–`D-218`): `benchmark.py` `9efea8da` on top (not pushed; 240 tests, pyright
and pre-commit clean; gurobipy checked by hand); `--retry-timeouts` added on the user's ask; exit codes 3 (`STOP`)
and 130 (Ctrl-C) accepted. Manual checks 2026-11-18 (`F-69`), then `D-219`: `51b8223e` (docstring) and `76bac866`
(events, a clean end on errors, the runner's processor time, clearer resumes), then `b1d434aa` (after the user's
review: `total` in events, `Opening`, no `repeats`) on top; 247 tests; all pushed 2026-11-18.
Step 8 designed and done 2026-11-18 (`D-220`–`D-222`, `F-70`): the summary, `e72c99cd` on top (pushed 2026-11-18; 261
tests); the comparison is step 8b (`D-221`), plots `T-1.23`; the colleague's questions `Q-65`–`Q-69` open; comparisons
and plots per task in `benchmark-runner.md` §11.
2026-11-18: 8b kept in this PR (`D-223`, output in `benchmark-runner.md` §12); step 9's file a generic example, not the
proof of `D-141` (`D-224`).
Step 8b done 2026-11-18 (`D-225`, `F-71`): `compare.py`, `19160d73` on top (the user's tidy of the script
folded in; pushed 2026-11-18; 285 tests). Step 9 done 2026-11-18 (`D-226`, `F-72`): `example.toml` and
the CHANGELOG entry, `ac832b35` on top (pushed 2026-11-18; 286 tests). PR #97 opened by the user
2026-11-18 (draft, base #88's branch, head `ac832b35`; description drafted with the user). Review done 2026-11-19
(`benchmark-review.md`, `F-73`): 286 tests pass, 12 blockers (e.g. `--overwrite` can delete a benchmark folder, a
resume skips crashed rounds), about 25 should-fix, integration gaps (gurobipy and `cpp-bindings` never run by a
test, ci-full not run on #97). Fixes decided (`D-227`–`D-232`, §6): six commits on top, done in one chat, one per prompt
(the user). Commit 1 done 2026-11-19: `1f823b59` (options; 315 tests); commit 2 `06ff47f7` (source tree; 322 tests);
commit 3 `35f42019` (runners; 346 tests; C101_25 and R201_25 LP as recorded); commit 4 `f96247b8` (`benchmark.py`; 366
tests; Linux and macOS signal paths not run: Docker Desktop off, left to ci-full); pre-commit and pyright clean; not
pushed. `../rcspp-dev/build` was configured from `../rcspp`: the guard now refuses it. B1 kept as `D-229` says: a crash
without a result still runs again on resume. Commit 5 `310b4b22` (summary and compare: `folder.py`; 390 tests); commit 6
`324fac42` (CI: `RCSPP_BENCHMARK_BUILD: build`, `-rs`). Fixes done 2026-11-19, head `324fac42`, pushed by the user; PR
description updated 2026-11-19 (commits 20-25, line counts, limits). First ci-full 2026-11-20 (`F-74`): the signal paths
pass on Linux and macOS; a quoting test written for Windows fails on POSIX; on Ubuntu one `cpp-bindings` run of C101_25
takes 25 iterations against 16 (objective equal). `b6edcb20` on top (not yet pushed when written): the test fixed, the
drift test names the layer; the user re-runs ci-full. Three chats in parallel (`D-233`; `D-234` proposed): this one
fixes ci-full, one sets up the gateway laptop (`T-1.24`), one starts `T-1.18`; #97 stays a draft for now. Proposed
2026-11-20: where to measure (`D-235`), the scripts at the root (`D-236`). Re-run read 2026-11-20: the drift is the C++
example layer on Linux, both of its masters alike (`F-74`). Traced 2026-11-20 (`F-75`): not reproduced locally (gcc 13
and 15 in Docker, sanitizers, valgrind clean); on the CI runner the example's HiGHS build returns duals 1e-13 from the
highspy wheel's on C101_25's degenerate seventh LP, and the pricer keeps other tied paths: not our code. `8b80353b` (the
layers' test: objective across layers, iterations within one HiGHS build, `D-237`) and `bd6822d4` (the time rule says
when iterations differ, `D-238`) on top, pushed 2026-11-20 (head `bd6822d4`, 393 tests); PR description updated (commits
26-28, line counts, the drift as a known limit). Debug branch, worktree and Docker volume removed. **Next:** the user
runs ci-full on #97, then out of draft when the user chooses;
then `T-1.18`, `T-1.19`, `T-1.16`, `T-1.21`, `T-1.20` (`D-171`), and `T-1.22` (presets with the colleagues).
Home server (`T-1.24`, the gateway laptop of `D-233`; `server-setup.md`, `home-server-runs.md`): set up 2026-11-19/20 in
the user's home without `sudo` (uv Python, highspy pinned to 1.15.1, Gurobi WLS); 389 benchmark tests pass at
`b6edcb20`; a looping LXD container left from a password recovery removed (load 0.00 after). `layers-50` (15 instances,
5 layer-master pairs, 3 rounds) **in progress** since 2026-11-20 05:10 UTC; partial (125 / 225 rounds): iterations and
columns equal across layers everywhere (no `F-75` drift with both HiGHS at 1.15.1), every pair within `D-139`'s rule,
R202_50 killed at 3600 s every time (`F-76`). Findings for #97 (`F-77`): the summary calls timeouts a second record,
`highspy` unpinned, two README build lines: fixed in `ee8b0272` on #97 (394 tests; pushed 2026-11-20, PR description
updated: commit 29, line counts, the pin under the HiGHS limit); per-iteration data written as it happens (`D-239`), a
`timeout_s` under the kill for hard instances (`D-240`). `layers-50` finished 2026-11-20 22:32 UTC (210 / 225 rounds, the
15 timeouts R202_50's, every pair), copied to `benchmark-results/colgen/home-server/layers-50/`; its `summary.md` is the
server's `b6edcb20` code's (its mixed and not-recorded flags are bugs fixed since, `ee8b0272` and `33c8a53d`).
**Next:** the user runs ci-full on #97 (`ee8b0272`); `layers-50` summarized again with current code, `compare.py` per
pair; the server moves to #97's head after it.
`T-1.18` (the Python VRP example's port) started 2026-11-20: design agreed (`D-241`–`D-249`: the example stays a small
program, `--master` required, `cg/` and `read_duals` go, a README and a docs section, rows `customer − 1`, integer
solve on with `--no-integer`, only `solve_vrp.py`). Small and mid tiers on the laptop (`F-78`, `F-79`,
`benchmark-results/colgen/vrp-port-{small,mid}/comparison.md`): LP equal, the layer faster everywhere (C201 18.7 s
against 29.5 s). Outside the master, the two cost the same per iteration where the search is heavy, the layer up to
0.9 ms more where it is cheap; totals follow the iterations (`F-80`). Branch `feature/column-generation-vrp-example` in
`../rcspp-dev` from #97's head (`D-250`, supersedes `D-246`; `drivers.py example` fails while it is checked out).
Commit 1 `335290a9` (`F-81`: tests 606 passed, pyright and pre-commit clean, `examples/python` added to both).
Commit 2 `91432461` (docs; Sphinx not built here, Doxygen's XML missing). Commit 3 `b28a8e5d`, the test that the
example and the benchmark build the same model (arcs, priced routes, starting routes; 403 benchmark tests). All three
pushed 2026-11-20; PR description drafted. **Next:** the user opens the PR (base #97's branch) and runs CI;
the long tier (R201, about 30 min a run) when the desktop or the home server is free; then `T-1.19`.
`T-1.19` (the C++ VRP example's port) started 2026-11-20: what the example has that the layer lacks (`F-82`: the
engine benchmarks' algorithms on one dual sequence, Boost, a dead duals dump, a compile-time relaxation, no pool; the
core tests read Solomon files with its reader; a CI workflow builds it with `USE_VRP`). Small tier on the laptop
(`F-83`, `benchmark-results/colgen/vrp-port-cpp/`; `drivers.py cpp` is dead, so three throwaway programs over the
checkout): LP equal, `D-148` passes, pricing equal on one dual sequence. Versions and build type: `F-84`. **Proposed,
for the user:** `D-251`–`D-259` (mid tier next, `examples/column_generation/vrp/`, `--master {gurobi,highs}`, the
engine benchmarks on the layer, a C++ `cpp-native` program of the benchmark, versions, the three legs' test, docs,
branch from the Python port's head, five commits). Accepted 2026-11-20: all but `D-252` (revised: `examples/cpp/`
mirroring `examples/python/`) and `D-254` (explained); real measures on an idle machine (#98). **Next:** the user
`D-254` amended by `D-260` (the engine benchmarks' features go to the benchmark: replay `Q-68`,
stage options, Boost in `cpp-native`; dump, gap, `--gh-dir`, `benchmark.yml` dropped); `Q-70` (a comparison module).
A colleague's open PRs (#41–#50, #63, #74, #82) edit `examples/cpp/` (`F-86`, `F-87`).
Decided (`D-262`, `D-263`): the port first with the engine benchmarks removed (their features: `T-1.25`,
`T-1.26`), cleaned up as it goes, five commits on `feature/column-generation-cpp-vrp-example` from #98's head, then a
rebase pass over the open PRs. Commit 1 `82e6badf` (the core tests' Solomon reader in `tests/cpp/vrp_subproblem/`,
`.gitignore`'s stale `instances/duals/` line dropped; 722 tests). Commit 2 `34923757` (the port; 759 C++ tests pass, the
C++ lint hooks cover `examples/cpp/`, no suppression). Commit 3 `f9995cc6` (docs). Commit 4 `33c8a53d` (the HiGHS that
solves and the build type). Commit 5 `a7838214` (the `cpp-native` leg: `cpp_native/cpp_native_solver.cpp`, its
runner, `highs` and `gurobi` masters; C101_25 equal to the recorded `cpp-bindings` runs, 16/599, 18/666, 23/691,
29/723; 759 C++ and 421 benchmark tests pass, one flaky first run of a VRP command-line test not reproduced; the user's
`example.toml` run with the layer, `F-89`, found a false mixed flag, fixed in it). The user reviewed the five
commits 2026-11-20: kept as they are, no change. **Next:** the PR (base #98); the measures of `D-251` on an idle
machine (#98).
2026-11-16: layers renamed `cpp-bindings` and `cpp-native` (`D-189`; `8e944acd` on
`feature/column-generation-benchmark`, in `../rcspp-dev`, not pushed). Slides for the colleagues in
`benchmark-results/colgen/presentation/benchmark/`. Review questions answered: masters per layer, checks
before running, presets `exact` only, `cpp-native`'s share, `from` (`D-190`–`D-194`, amending steps 2–3).
History: `decisions.md`; measurements and how to run them: `benchmark-results/colgen/HANDOFF.md`.
Baseline: `main` @ `129b15e0`. CI is out of the plan for now (`D-52`).

## New chat

One task per chat. Paste this, then one sentence for the task (its roadmap row is the full brief):

```text
Column generation work in rcspp, branch feature/column-generation. Read plans/colgen/README.md first and
follow its agent protocol; open other plan files only when the task needs them. Before ending, update the
README status and the task's roadmap row.
Task:
```

## Agent protocol

- **Hard rule (`D-55`):** nothing that stays (code, comments, tests, commit messages, PR and issue texts, `docs/`)
  mentions `plans/colgen`, its files, its IDs or plan-only words ("phase 1", "tier 2"). Write the reason instead.
- Read this file first. Open the others only when the task needs them (see Index).
- Before a chat ends: record decisions (`D-`) and findings (`F-`), update the status above and the task's roadmap row,
  so the next chat starts from them alone.
- Refer to items by ID (`D-`, `Q-`, `P-`, `F-`, `T-`). Do not restate another file's content: link it.
- Code facts belong in `inventory.md`, with `path:line`. Code moves, so re-check a fact before relying on it.
- Q answered → add a `D-` in `decisions.md`, then mark the Q `→ D-xx`. Accepted D are never edited:
  supersede them with a new D.
- Style: tables and bullets, no intros, at most ~200 lines per file, 120 columns (markdownlint `MD013`).
- **Reminders to raise with the user** (don't decide alone):
  - any piece that moves sparse data (masters, pools, arc flows, Python boundary): numpy vs `scipy.sparse` vs
    C++ CSR (`D-16`);
  - any new name: full words (`D-12`).
- These files are not shipped docs. User-facing docs go in `docs/` once the code exists.

## Goal

A **generic** column generation (CG) layer (branch-and-price designed but not implemented yet, `D-20`), built on
the `rcspp` pricing engine. Usable from Python; the C++ layer is an example to copy (`D-142`). Built from
switchable components (LP solver, pricing strategy, stabilization…).
Fast by default. Diagnostics are cheap or can be switched off. It must support a benchmark project that compares
configurations.

## Scope

| in | out (for now) |
|---|---|
| Generic master: rows with `=`/`≥`/`≤`, columns = `rcspp::Column` | Anything VRP-specific in the library (VRP stays in examples and tests) |
| LP backends: HiGHS, Gurobi; SCIP later (`D-05`, `D-06`) | Non-robust cuts (they need new resources) |
| CG loop, pricing strategies, stabilization, column pool | Automatic decomposition (GCG-style) |
| B&P with arc branching (phase 3: design only, `D-20`; SCIP study `Q-22`) | |

## Index

Colleague-facing summaries (2026-10-21): French <https://claude.ai/artifact/CZfDgEKG37NtHWKCcpj2Ab>, English
<https://claude.ai/artifact/BZLej9R9NdVA4dcYt6joCf>. Keep both in sync with `decisions.md` (not yet updated from
`D-58` on). Colleague pages of 2026-10-26: SCIP pros and cons, SoPlex vs HiGHS (links in `prior-art.md`).

| file | contains | read when |
|---|---|---|
| `inventory.md` | existing pieces `P-`, facts and pitfalls `F-` | before designing or coding anything |
| `architecture.md` | components, interfaces, how they plug into the existing code, perf and stats design (early proposal: names and details older than the code) | design and implementation |
| `api-sketch.md` | what user code would look like (C++, Python, configs, B&P, custom backend; early proposal, older than the code) | discussing or reviewing the API |
| `phase-1-design.md` | detailed design of phase 1 in C++: signatures, layout, tests, PR breakdown and commits, proposals `DP-` | before implementing phase 1 |
| `phase-1-python.md` | the Python layer (PRs 5–6): bindings, graph pricers, master solvers over the user's model, packaging, tests, commits | before PR 5 |
| `roadmap.md` | phases, tasks `T-`, acceptance criteria | picking up work |
| `decisions.md` | decisions `D-` | before proposing something that may already be decided |
| `questions.md` | open questions `Q-` | before deciding; when blocked |
| `prior-art.md` | SCIP, GCG, Coluna, RouteOpt, HiGHS, python-mip…: comparison + proposals `PA-` | before designing a component |
| `issues.md` + `issues/` | GitHub issues to create (`I-`, one paste-ready file each) and to close | when a finding is outside the plan's code |
| `baseline.md` | measured benchmark results (informative and golden) | before and after perf-sensitive changes |
| `packaging.md` | study notes: Gurobi from Python (B1, B2, C API, adapters), Python packages, C++ packaging and flags | before `T-1.11`, `T-1.12`, or any packaging change |
| `python-layer.md` | the pure-Python layer (`T-1.15`): contents, fix A + B, pool option, commits, comparison with the C++ layer | before coding `T-1.15` |
| `python-layer-records.md` | the pure-Python layer's step records and review findings (B., D., F.) | before fixing or reviewing that layer |
| `python-layer-runs.md` | the pure-Python layer's runs against the C++-backed one (step G): LP values, times, probes of B.15, B.16, D.12, the `ColumnBatch` copy | before deciding those items, planning the full comparison |
| `python-layer-comparison.md` | step H: where the pure layer's extra time goes, the full comparison matrix, `Q-41`'s rule, D.12 after heuristic runs | before running the full comparison or deciding `Q-41` |
| `separation.md` | the colleagues' feedback of 2026-11-09, C++ layer as an example: what moves, open questions, PR shapes, VRP comparisons | before `T-1.17`–`T-1.19` |
| `fold-map.md` (+ `-commits`, `-targets`, `tools/fold_filter.py`) | the stack's rewrite into `examples/column_generation/`: method, filter, every commit, where each item goes, PR heads, final state | before rewriting the stack |
| `fold-record.md` | the rewrite as done: heads, checks, departures, the Linux run, step 6 | before the push or a stack review |
| `fold-review.md` | the read of the eight folded PRs: heads, findings `V.`, coverage, questions | before the fixups or the merge |
| `benchmark-runner.md` | `T-1.10`'s runner: shape, TOML input, output folders, records, statistics, growth per task, coding steps | before coding the runner or a follow-up's option |
| `benchmark-review.md` | the read of PR #97: checks run, blockers, should-fix, test gaps, docs, proposal | before fixing #97 |
| `benchmark-solutions.md` | the runner's solutions: `--overwrite`, `solution.json`, the integer solve, the instance's resolved solution, per-iteration data | before step 5's fixup, the integer solve or per-iteration data |
| `measurements.md` | the measurement scripts for the 2026-10-30 presentation: matrix, runner requirements, facts, environment | before writing or running them |
| `server-setup.md` | the home server (`T-1.24`): the machine, choices, every setup step from scratch, updating, log | setting up or updating a benchmark machine |
| `home-server-runs.md` | the home server's benchmarks and their analysis (`layers-50`), findings for the benchmark | before reading or running a benchmark there |

## Project conventions that apply here

- C++23, core in `cpp/rcspp/` (mostly headers; non-template code in `.cpp`, `D-56`), namespace `rcspp`, snake_case, clang-format/clang-tidy/cpplint/cppcheck.
- Python: pybind11 bindings in `python/bindings/`, package in `python/src/rcspp/`, black/isort/flake8.
- CMake options are named `USE_*` (`USE_PYTHON`, `USE_VRP`, `USE_TESTS`, `USE_COVERAGE`).
- Coverage gates in CI: **100 % lines** for `cpp/rcspp/` and `python/bindings/` (`gcovr.cfg`,
  `scripts/check_cpp_coverage.py`), **90 %** for Python (`ci.yml`).
- Conventional commits (`feat(scope): …`, a pre-commit hook enforces them). Large features go in stacked PRs, each
  building and passing tests at its head.
- Tests: GoogleTest in `tests/cpp/`, pytest in `tests/python/`. Solomon VRPTW instances in `instances/` (git LFS).

## Glossary

| term | meaning |
|---|---|
| RMP | restricted master problem: the master LP over the columns generated so far |
| duals `π` | one per master row, from the LP solver. Pricing turns them into reduced costs |
| pricing | find columns with negative reduced cost (here: an RCSPP solve on a `ResourceGraph`) |
| exact proof | a pricing solve that finished (`AlgorithmStatus::COMPLETE`) and found nothing: the LP is optimal |
| stabilization | smooth the duals between iterations to avoid oscillation and tailing-off |
| Lagrangian bound | lower bound available before CG converges: `z_RMP + Σ_k U_k · min_rc_k` |
| B&P | branch-and-price: branch-and-bound where every node runs CG |
| backend, master solver | adapter to an external LP/MIP solver (HiGHS, Gurobi, SoPlex). The interface `SolverBackend` is renamed `MasterSolver` (`D-67`); older plan text keeps the old name |
