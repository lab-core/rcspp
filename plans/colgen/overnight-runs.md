# Overnight runs of 2026-11-20/21: `T-1.19` on the laptop, `T-1.18` on the home server

One task per machine (`D-264`): each runs one chain, so neither waits on the other and nothing needs starting during
the night. Gurobi's WLS allows 2 sessions: the two machines take both (they shared it without trouble on
2026-11-20), so the desktop stays off Gurobi. Times from either machine are not the desktop's reference (`D-235`,
`D-245`): equivalence (LP, iterations, columns) holds anywhere, times are indicative.

## Laptop: all of `T-1.19` (`D-251`), about 3–5 h

What runs, in order:

| # | run | output | estimate |
|---|---|---|---|
| L1 | old example against the layer, small tier (C101_25, R101_25, R201_25, RC201_12), 5 rounds | `vrp-port-cpp/idle-small.jsonl` | 5 min |
| L2 | the same, mid tier (C101_50, R201_50, RC101_50, C201), 3 rounds | `idle-mid.jsonl` | 15–30 min |
| L3 | replay (pricing on one dual sequence), small 5 rounds and mid 3 rounds | `idle-replay-{small,mid}.jsonl` | 20–40 min |
| L4 | the benchmark's three legs, `cpp-legs.toml`: 8 instances (L1 + L2's) × 7 layer-master pairs × 3 rounds, single-customer start, threads 1 | `../rcspp-dev/benchmarks/column_generation/results/cpp-legs/` | 1–2 h |
| L5 | old against the layer, R201, 2 rounds (the long tier, as `T-1.18`'s; beyond `D-251`, last so it blocks nothing) | `idle-long.jsonl` | 1–2 h |

L1–L3 and L5 are `F-83`'s programs (`benchmark-results/colgen/vrp-port-cpp/build/Release/`), not rebuilt: the same
binaries as the busy run. L4 needs `build-python-highs` rebuilt first: its `_core` (16:30) is older than `33c8a53d`.

Before: plugged in, sleep off when plugged in (Settings → Power → never; or `powercfg /change standby-timeout-ac 0`,
put back afterwards), VS Code, Chrome and OneDrive sync closed or paused, Windows Update paused for a day.

Step 1, rebuild (PowerShell, a few minutes):

```powershell
cd C:\Users\marbe\OneDrive\Desktop\programming\git\rcspp-dev
git status --short          # only benchmarks/column_generation/cpp-legs.toml (untracked)
cmake --build build-python-highs --config Release
cd benchmarks\column_generation
..\..\..\rcspp\.venv\Scripts\python.exe benchmark.py cpp-legs.toml --build build-python-highs --machine leo-laptop --check options
```

The check lists 56 runs × 3 rounds and stops before running. `cpp-legs.toml` (already written there):

```toml
# The three layers on the C++ example's small and mid tiers: each master, exact pricing, the example's start.
[benchmark]
repeats = 3
timeout = 1800

[defaults]
start = "single-customer"
column_generation = { threads = 1 }

[matrix]
instance = ["C101_25", "R101_25", "R201_25", "RC201_12", "C101_50", "R201_50", "RC101_50", "C201"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["exact"]
```

Step 2, the chain (paste as one block in a PowerShell window left open):

```powershell
$py  = "C:\Users\marbe\OneDrive\Desktop\programming\git\rcspp\.venv\Scripts\python.exe"
$cpp = "C:\Users\marbe\OneDrive\Desktop\programming\git\rcspp\benchmark-results\colgen\vrp-port-cpp"
$ben = "C:\Users\marbe\OneDrive\Desktop\programming\git\rcspp-dev\benchmarks\column_generation"
$small = "C101_25,R101_25,R201_25,RC201_12"; $mid = "C101_50,R201_50,RC101_50,C201"
Set-Location $cpp
& $py run.py --instances $small --rounds 5 --output idle-small.jsonl
& $py run.py --instances $mid --rounds 3 --output idle-mid.jsonl
& $py run.py --instances $small --rounds 5 --replay --output idle-replay-small.jsonl
& $py run.py --instances $mid --rounds 3 --replay --output idle-replay-mid.jsonl
Set-Location $ben
& $py benchmark.py cpp-legs.toml --build build-python-highs --processes 1 --machine leo-laptop
Set-Location $cpp
& $py run.py --instances R201 --rounds 2 --output idle-long.jsonl
"done $(Get-Date)"
```

`run.py` appends: after an interruption, delete the partial `.jsonl` of the step it was in and start from that step.
The benchmark resumes by itself (the same command again).

Morning: `run.py --summarize idle-small.jsonl` (and `idle-mid`, `idle-long`), `--summarize-replay
idle-replay-small.jsonl` (and `-mid`); `results/cpp-legs/summary.md`; `compare.py` per instance and master
(`cpp-native` against `cpp-bindings` `highs`, `gurobi` against `gurobipy`).

## Home server: all of `T-1.18` (`D-245`), about 5–7 h

The old Python example (python-mip) against the pure layer with gurobipy, `drivers.py` and `run_matrix.py` as on the
laptop (`F-78`, `F-79`), on the server's checkout `b6edcb20` (#97 before `ee8b0272`; the old example and the pure layer
are there, its `build-python` current). The server must stay on `b6edcb20` until the chain ends: the C++ port's
branch has no old example.

| # | run | estimate |
|---|---|---|
| S1 | small tier, 5 rounds | 10 min |
| S2 | mid tier, 3 rounds | 20–40 min |
| S3 | long tier: R201, single-customer start, 2 rounds | 4–6 h (30 min a run on the desktop, 2–3 times slower here) |

Step 1, the drivers and python-mip (ssh session):

```bash
cd ~/rcspp && source .venv/bin/activate
git log --oneline -1                                   # b6edcb20
git fetch origin feature/column-generation
mkdir -p ~/vrp-port-server && cd ~/vrp-port-server
git -C ~/rcspp show FETCH_HEAD:benchmark-results/colgen/drivers.py > drivers.py
git -C ~/rcspp show FETCH_HEAD:benchmark-results/colgen/run_matrix.py > run_matrix.py
uv pip install mip==2.0.0                              # the laptop's version
python -c "import mip; m = mip.Model(solver_name=mip.GRB); print('GRB ok')"
```

**Stop if the last line fails**: the example would fall back to CBC and measure another master. Paste the error.

Step 2, the environment and a check on C101_25 (both LP 191.81362; the example's log says `solver: GRB`):

```bash
cat > ~/vrp-port-server/env.sh <<'EOF'
source ~/rcspp/.venv/bin/activate
export COLGEN_SOURCE_DIRECTORY="$HOME/rcspp"
export COLGEN_INIT=single
export PYTHONPATH="$HOME/rcspp/build-python/python:$HOME/rcspp/python/src:$HOME/rcspp/examples/column_generation/python"
EOF
cd ~/vrp-port-server && source env.sh
python drivers.py example --instance C101_25 | grep -E "solver|RESULT"
python drivers.py layer --loop purepython --master gurobipy --instance C101_25 | grep RESULT
```

Step 3, the chain in `tmux`:

```bash
cat > ~/vrp-port-server/run-all.sh <<'EOF'
#!/usr/bin/env bash
cd ~/vrp-port-server && source env.sh
configs="example-mip,layer-purepython-gurobipy-exact"
python run_matrix.py --output small --instances C101_25,R101_25,R201_25,RC201_12 --configs "$configs" --repeats 5
python run_matrix.py --output mid --instances C101_50,R201_50,RC101_50,C201 --configs "$configs" --repeats 3
python run_matrix.py --output long --instances R201 --configs "$configs" --repeats 2
echo "done $(date -u)"
EOF
chmod +x ~/vrp-port-server/run-all.sh
tmux new -s t118 "~/vrp-port-server/run-all.sh 2>&1 | tee ~/vrp-port-server/run-all.log; exec bash"
# Ctrl-b d to leave; tmux attach -t t118 to look; the session stays open after the chain ends
```

Progress: `cat ~/vrp-port-server/long/status.txt`. Each `run_matrix.py` resumes an existing folder: after an
interruption, run `run-all.sh` again. Morning, from the laptop (into `benchmark-results/colgen/`):
`scp -r leo@<server>:~/vrp-port-server/{small,mid,long} vrp-port-server/`.

## Next steps (when a chain has ended)

| machine | step | file |
|---|---|---|
| server | `layers-50`'s summary with current code and `compare.py` per pair (laptop, minutes) | — |
| server | after S3 only: move to `feature/column-generation-cpp-vrp-example` (`e91db2963`), `git lfs pull`, submodule, rebuild `build-python` (its Gurobi option is on: `cpp-native`'s `gurobi`), the benchmark's tests | `server-setup.md` § Updating |
| server | `layers-50` with `cpp-native`: 14 instances × 7 pairs × 3 rounds (98 runs, about 3.5 h), R202_50 apart | `layers-50-legs.toml` |
| server | R202_50 on `cpp-native`, `column_generation.timeout_s = 3300` (`D-240`): how far it gets (about 2 h) | `r202-50.toml` |
| either | `T-1.20` dry run: R201_50, exact against an exact stage stopped after 1 or 5 columns then a full one, 7 pairs, 1 round (21 runs) | `truncated-50.toml` |

The three files (checked with `--check options` on the laptop: 98, 2, 21 runs), to write next to `benchmark.py`:

```toml
# layers-50-legs.toml: the three layers on the 50-customer instances (R202_50 apart), each master, exact, 3 rounds.
[benchmark]
repeats = 3
timeout = 3600
[defaults]
start = { type = "artificial" }
column_generation = { threads = 1 }
[matrix]
instance = ["C101_50", "C102_50", "C103_50", "C201_50", "C202_50",
            "R101_50", "R102_50", "R103_50", "R105_50", "R107_50", "R201_50",
            "RC101_50", "RC102_50", "RC201_50"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["exact"]
```

```toml
# r202-50.toml: R202_50, which never finished in an hour; the run stops itself at 55 min and records how far it got.
[benchmark]
repeats = 1
timeout = 3600
[defaults]
start = { type = "artificial" }
column_generation = { threads = 1, timeout_s = 3300 }
[matrix]
instance = ["R202_50"]
layer = ["cpp-native"]
master = ["highs", "gurobi"]
pricing = ["exact"]
```

```toml
# truncated-50.toml: many cheap iterations, an exact search stopped after k columns, then a full one for the proof.
[benchmark]
repeats = 1
timeout = 3600
[defaults]
start = "single-customer"
column_generation = { threads = 1 }
[pricing.first-1]
stages = [{ algorithm = "simple", max_solutions = 1 }, { algorithm = "simple" }]
[pricing.first-5]
stages = [{ algorithm = "simple", max_solutions = 5 }, { algorithm = "simple" }]
[matrix]
instance = ["R201_50"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["exact", "first-1", "first-5"]
```

Run each as `server-setup.md` § 8's general form (`tmux`, `--processes 1`, `--machine home-server`); on the laptop
with `--build build-python-highs --machine leo-laptop`.

## Log

| date | what |
|---|---|
| 2026-11-20 | planned; `cpp-legs.toml` written in `../rcspp-dev/benchmarks/column_generation/` (untracked), checked |
| 2026-11-21 | both chains finished (server 07:47 UTC, laptop's benchmark 05:04 UTC, R201 03:57); read: `F-90`, `F-91`, `F-92`; `cpp-legs` copied to `benchmark-results/colgen/cpp-legs/` |
