# Home server: benchmark runs and their analysis

Runs on the home server (`server-setup.md`): Celeron 2950M, 2 cores, `--processes 1`, `column_generation.threads = 1`,
commit `b6edcb20`, highspy and HiGHS 1.15.1, gurobipy 13.0.1. Results copied to
`benchmark-results/colgen/home-server/` (untracked).

## `layers-50`: the two layers on the 50-customer instances

15 instances × (`package`: `highspy`, `gurobipy`; `cpp-bindings`: `highspy`, `gurobipy`, `highs`) × 3 rounds, exact
pricing, artificial start, a 3600 s kill. Started 2026-11-20 05:10 UTC.

### Partial analysis (copy of 14:15 UTC: 125 / 225 rounds, rounds 1 and most of 2)

- **Equal everywhere:** every finished instance gives the same status (`Optimal`), objective, iterations and
  columns on both layers, per master; `highs` and `highspy` equal too (same HiGHS 1.15.1).
- **Times equivalent by the rule max(5 %, 0.25 s) on every pair so far.** Closest: C201_50 `highspy` +0.26 s
  (+4.7 %, limit 0.28 s). Small instances (< 1 s) show +10 to +37 % for `package`, 0.02-0.10 s, under the floor:
  1.5-1.9 ms per iteration (C101, R101, RC101), as measured on the desktop before.
- **R202_50: every run killed at 3600 s**, any layer or master (7 of 7 so far). The runners print nothing during a
  run (`log.txt` empty), so a timeout says nothing of how far it got.
- Where the time goes: pricing dominates the hard instances (C202_50: 377-506 s, 97 % pricing); C201_50 is the
  exception, its master 65 % of 5.5 s (13,000-17,000 columns).
- Masters: HiGHS and Gurobi take other paths (C201_50: 60 against 119 iterations); neither is always faster
  (C202_50: HiGHS 377 s, Gurobi 500 s; C103_50: Gurobi 5.6 s, HiGHS 6.5 s).
- Machine: other processes 1.3-11.7 % (median 4.2 %) during the runs, after the LXD loop's removal; no battery.

| instance | `highspy` it. / col. | `gurobipy` it. / col. | `highspy` s (pkg / cpp) | `gurobipy` s (pkg / cpp) |
|---|---|---|---|---|
| C101_50 | 26 / 1288 | 35 / 837 | 0.24 / 0.20 | 0.27 / 0.25 |
| C102_50 | 81 / 5613 | 91 / 5951 | 17.24 / 16.60 | 18.77 / 18.59 |
| C103_50 | 40 / 3323 | 47 / 3391 | 6.44 / 6.58 | 5.68 / 5.59 |
| C201_50 | 60 / 13002 | 119 / 17051 | 5.77 / 5.51 | 5.53 / 5.35 |
| C202_50 | 76 / 25656 | 153 / 29192 | 386.5 / 377.0 | 499.8 / 505.7 |
| R101_50 | 20 / 456 | 17 / 422 | 0.12 / 0.09 | 0.11 / 0.10 |
| R102_50 | 18 / 1303 | 19 / 1266 | 0.63 / 0.59 | 0.70 / 0.70 |
| R103_50 | 26 / 1679 | 24 / 1650 | 1.51 / 1.45 | 1.49 / 1.50 |
| R105_50 | 20 / 804 | 18 / 765 | 0.23 / 0.20 | 0.22 / 0.21 |
| R107_50 | 23 / 1910 | 22 / 1722 | 1.99 / 1.92 | 1.93 / 1.91 |
| R201_50 | 33 / 17929 | 25 / 18249 | 57.3 / 56.9 | 55.9 / 56.2 |
| R202_50 | timeout | timeout | > 3600 | > 3600 |
| RC101_50 | 27 / 760 | 25 / 742 | 0.25 / 0.20 | 0.24 / 0.22 |
| RC102_50 | 34 / 1341 | 37 / 1257 | 0.93 / 0.83 | 1.02 / 0.97 |
| RC201_50 | 37 / 11497 | 40 / 12929 | 17.36 / 17.15 | 19.92 / 19.94 |

Medians over the rounds finished (2, RC instances 1); `highs` within 3 % of `cpp-bindings` `highspy` everywhere.

- **No `F-75` drift here:** on Ubuntu (gcc 13.3, Intel) with both HiGHS at 1.15.1 (highspy pinned), iterations and
  columns are equal across layers on every run, C101_25 included (16 / 599). CI's package ran highspy 1.16.0; as
  Windows and macOS did not drift with it, the version gap is a suspect, not a proof (`F-76`).

### Findings for the benchmark (PR #97) (`F-77`): 1-4 fixed in `ee8b0272`, 5 decided

1. **The summary calls a folder "mixed" because of its timeouts.** A round killed at its timeout (and one in
   progress) has no record; the summary counts that as a second record ("mixed: 2 records", every field `-> None`)
   and flags "solver version not recorded" for those rounds. A round without a record should not make the folder
   mixed, nor lack a version.
2. **`requirements.txt` leaves `highspy` unpinned:** a new environment gets 1.16.0, the example builds 1.15.1.
3. **The README's build command lacks `-DCMAKE_CXX_CLANG_TIDY=`** (clang-tidy on by default; CI passes it).
4. **The README adds `USE_COLUMN_GENERATION_EXAMPLE_GUROBI` "when Gurobi is installed"**, but the benchmark never uses
   it: both layers reach Gurobi through gurobipy.
5. **A long run leaves no trace of its progress**: nothing per iteration in `log.txt`, so a timeout gives no
   iterations or bound. Decided: per-iteration data written as it happens (`D-239`); hard instances with a
   `column_generation.timeout_s` under the kill (`D-240`).

### Next

- The final folder (expected ~23:00 UTC 2026-11-20): `summary.py`, then `compare.py` per instance and master with
  `--time-within 5%` (exit codes for the verdict), every round in.
- R202_50 in later benchmarks: a longer kill, a `column_generation.timeout_s`, or left out.
