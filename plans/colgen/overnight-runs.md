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

## 2026-11-21: the laptop's dry run and probe, the server's second run

| # | run | machine | file | status |
|---|---|---|---|---|
| L6 | `T-1.20` dry run: R201_50, exact against an exact stage stopped after 1 or 5 columns then a full one, 7 pairs, 1 round, each run stopped at 10 min (`timeout_s = 600`) | laptop | `truncated-50.toml` (now in `benchmark-results/colgen/truncated-50/`) | done: `F-97` |
| L7 | `Q-71` probe: the search with and without preprocessing, plain, ng-8 and elementary routes | laptop, after L6 | `cg-only/search_probe.py --relaxation` | done with ng-8 and ng-16: `F-98` (elementary too slow) |
| S4 | the three layers on the 50-customer instances: 14 instances (R202_50 apart) × 7 pairs × 3 rounds, 98 runs, about 3.5 h | server | `layers-50-legs.toml` | done, results still on the server |
| S5 | R202_50 on `cpp-native`, both masters, stopped at 55 min (`D-240`): how far it gets, about 2 h | server, after S4 | `r202-50.toml` | done, results still on the server |

The laptop's runs share it with VS Code and this chat: L6's iterations hold, its times are indicative; L7's ratios
hold (every variant in one process on the same duals). Gurobi's 2 WLS sessions: the laptop and the server.

### Server, step 1: move to the C++ port's head and rebuild (in `tmux`, 30-60 min)

```bash
tmux new -s build
cd ~/rcspp && source .venv/bin/activate
git status --short                     # only untracked files (layers-50.toml)
git fetch origin
git checkout -b feature/column-generation-cpp-vrp-example origin/feature/column-generation-cpp-vrp-example
git log --oneline -1                   # e91db2963 or later
git lfs pull && git submodule update --init --recursive
uv pip install -r benchmarks/column_generation/requirements.txt
cmake --build build-python --parallel 2 2>&1 | tail -3
ls build-python/bin/cpp-native-solver  # the cpp-native layer's program (the Gurobi option is on: its gurobi master)
RCSPP_BENCHMARK_BUILD=build-python pytest -rs -q benchmarks/column_generation/tests | tail -5
```

Expected: every test passes (421 on the laptop), skips only for a missing piece. Paste the tail if anything fails.

### Server, step 2: the two files, checked

```bash
cd ~/rcspp/benchmarks/column_generation
cat > layers-50-legs.toml <<'EOF'
# The three layers on the 50-customer instances (R202_50 apart), each master, exact pricing, 3 rounds.
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
EOF
cat > r202-50.toml <<'EOF'
# R202_50, which never finished in an hour: the run stops itself at 55 min and records how far it got.
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
EOF
source ~/rcspp/.venv/bin/activate
python benchmark.py layers-50-legs.toml --machine home-server --check options | grep -v dropped | head -2   # 98 runs
python benchmark.py r202-50.toml --machine home-server --check options | grep -v dropped | head -2         # 2 runs
```

### Server, step 3: the chain

```bash
cat > ~/run-legs.sh <<'EOF'
#!/usr/bin/env bash
cd ~/rcspp/benchmarks/column_generation && source ~/rcspp/.venv/bin/activate
python benchmark.py layers-50-legs.toml --processes 1 --machine home-server
python benchmark.py r202-50.toml --processes 1 --machine home-server
echo "done $(date -u)"
EOF
chmod +x ~/run-legs.sh
tmux new -s legs "~/run-legs.sh 2>&1 | tee ~/run-legs.log; exec bash"
# Ctrl-b d to leave; tmux attach -t legs to look; the same script again resumes a stopped benchmark
```

Results back, from the laptop, into `benchmark-results/colgen/home-server/`:
`scp -r leo@<server>:~/rcspp/benchmarks/column_generation/results/{layers-50-legs,r202-50} .`

Also pending: `layers-50`'s summary with current code and `compare.py` per pair (laptop, minutes).

## 2026-11-21: `T-1.20`'s matrix on the server (`D-268`)

| # | run | file | estimate |
|---|---|---|---|
| S6 | exact pricing on the 1-types: C101_25, C102_25, R101_25; C101-C103, R101-R103, R105, R107, RC101, RC102 at 50; C101, R101, RC101 at 100 (the files that exist: the benchmark cannot cut an instance) × 7 pairs × 3 rounds (336 runs) | `t120-exact.toml` | about 1 h |
| S7 | tabu then exact on R201_50, RC201_50 × 7 pairs × 3 rounds (42 runs) | `t120-tabu-50.toml` | under 1 h |
| S8 | tabu then exact on R201 × 7 pairs × 3 rounds (21 runs), each stopped at 55 min | `t120-tabu-r201.toml` | 2-19 h: last, stop it with `touch results/t120-tabu-r201/STOP` if needed |

The server stays on the C++ port's head of S4 (`e91db2963`); S4-S5's folders stay where they are. The verdict
(`D-268`): per cell, the pure layer's extra time an iteration against the C++ layers', beside `D-139`'s rule.

```bash
cd ~/rcspp/benchmarks/column_generation && source ~/rcspp/.venv/bin/activate
git log --oneline -1                   # e91db2963 (or later on the same branch)
cat > t120-exact.toml <<'EOF'
# Many cheap iterations: exact pricing on the 1-types (every file of C1, R1, RC1 at 25 and 50 customers, and C101,
# R101, RC101 at 100), the three layers, each master, 3 rounds; each run stopped at 10 min.
[benchmark]
repeats = 3
timeout = 900

[defaults]
start = "single-customer"
column_generation = { threads = 1, timeout_s = 600 }

[matrix]
instance = ["C101_25", "C102_25", "R101_25",
            "C101_50", "C102_50", "C103_50", "R101_50", "R102_50", "R103_50", "R105_50", "R107_50",
            "RC101_50", "RC102_50",
            "C101", "R101", "RC101"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["exact"]
EOF
cat > t120-tabu-50.toml <<'EOF'
# Many cheap iterations: a capped tabu first stage then exact, on the R2 types at 50 customers, where it is the
# faster setup; the three layers, each master, 3 rounds; each run stopped at 10 min.
[benchmark]
repeats = 3
timeout = 900

[defaults]
start = "single-customer"
column_generation = { threads = 1, timeout_s = 600 }

[pricing.tabu-first]
stages = [{ algorithm = "tabu", max_solutions = 20, max_iterations = 1000 }, { algorithm = "simple" }]

[matrix]
instance = ["R201_50", "RC201_50"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["tabu-first"]
EOF
cat > t120-tabu-r201.toml <<'EOF'
# Many cheap iterations: the capped tabu first stage then exact on R201 (100 customers), the three layers, each
# master, 3 rounds; each run stops itself at 55 min and records how far it got. Last: stop it with a STOP file.
[benchmark]
repeats = 3
timeout = 3600

[defaults]
start = "single-customer"
column_generation = { threads = 1, timeout_s = 3300 }

[pricing.tabu-first]
stages = [{ algorithm = "tabu", max_solutions = 20, max_iterations = 1000 }, { algorithm = "simple" }]

[matrix]
instance = ["R201"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["tabu-first"]
EOF
for f in t120-exact t120-tabu-50 t120-tabu-r201; do
  python benchmark.py $f.toml --machine home-server --check options | grep -v dropped | head -1
done                                   # 112, 14 and 7 runs, 3 rounds
cat > ~/run-t120.sh <<'EOF'
#!/usr/bin/env bash
cd ~/rcspp/benchmarks/column_generation && source ~/rcspp/.venv/bin/activate
for f in t120-exact t120-tabu-50 t120-tabu-r201; do
  python benchmark.py $f.toml --processes 1 --machine home-server
done
echo "done $(date -u)"
EOF
chmod +x ~/run-t120.sh
tmux new -s t120 "~/run-t120.sh 2>&1 | tee ~/run-t120.log; exec bash"
```

Progress: `cat results/t120-exact/status.txt` (and the others). Results back later, from the laptop, into
`benchmark-results/colgen/home-server/`:

```bash
scp -r 'leo@<server>:~/rcspp/benchmarks/column_generation/results/{layers-50-legs,r202-50,t120-exact,t120-tabu-50}' .
scp -r 'leo@<server>:~/rcspp/benchmarks/column_generation/results/t120-tabu-r201' .
```

## Log

| date | what |
|---|---|
| 2026-11-20 | planned; `cpp-legs.toml` written in `../rcspp-dev/benchmarks/column_generation/` (untracked), checked |
| 2026-11-21 | both chains finished (server 07:47 UTC, laptop's benchmark 05:04 UTC, R201 03:57); read: `F-90`, `F-91`, `F-92`; `cpp-legs` copied to `benchmark-results/colgen/cpp-legs/` |
| 2026-11-21 | L6 and L7 done on the laptop (`F-97`, `F-98`); the server's S4-S5 written up and started by the user |
| 2026-11-21 | S4-S5 done (the user); S6-S8 written (`T-1.20`'s matrix), checked on the laptop: 112, 14, 7 runs |
| 2026-11-22 | S6-S8 done (S6 18 min, S7 5 min, S8 2 h); the five folders back (archives, extracted into `benchmark-results/colgen/home-server/`); read: `F-101`-`F-103` |
| 2026-11-22 | the laptop's three TOMLs (`cpp-legs`, `truncated-50`, `pilot-cheap`) kept next to their results in `benchmark-results/colgen/<name>/`, removed from `../rcspp-dev` (untracked there, not part of the C++ port's PR) |
