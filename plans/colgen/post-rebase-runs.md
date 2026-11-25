# The server after the rebase onto the kernel stack (2026-11-24)

Not urgent (the user, 2026-11-24): the layers share the engine, so their comparison holds. The check: the same runs as
before the rebase, on the same machine, with LP, iterations and columns compared and times put side by side. Where
the kernel can move things: the per-node quota (Simple and A\* extend fewer labels), a bidirectional
`max_iterations` capping the whole search, tabu's `Truncated` status (capped stages).

Each file copies an earlier server file's `[benchmark]` and `[defaults]` exactly, so run names (with their options'
hash) are the same and `compare.py` takes them pairwise. The old results are in `benchmark-results/colgen/home-server/`.

| # | run | copy of | runs × rounds | estimate |
|---|---|---|---|---|
| S9 | exact, artificial start: C101_50, C201_50, R101_50, R201_50, RC101_50 | `layers-50-legs` (S4) | 35 × 3 | 1-1.5 h |
| S10 | cheap iterations, exact, single-customer start: C101_25, R101_25, C101_50, R101_50, RC101_50, R101 | `t120-exact` (S6) | 42 × 3 | 5-10 min |
| S11 | capped tabu then exact: R201_50, RC201_50 | `t120-tabu-50` (S7) | 14 × 3 | 5 min |

7 pairs per instance: `package` × {highspy, gurobipy}, `cpp-bindings` × {highspy, gurobipy, highs}, `cpp-native` ×
{highs, gurobi}. Gurobi's WLS has 2 sessions: the laptop may use one meanwhile.

## Step 1: the server on the stack's head, rebuilt (in `tmux`, 15-45 min)

```bash
tmux new -s build
cd ~/rcspp && source .venv/bin/activate
git status --short                     # only untracked files (the earlier *.toml)
git fetch origin
git switch -c feature/column-generation-vehicle-types --track origin/feature/column-generation-vehicle-types
git log --oneline -1                   # 71c13451
git lfs pull && git submodule update --init --recursive
uv pip install -r benchmarks/column_generation/requirements.txt
cmake --build build-python --parallel 2 2>&1 | tail -3
ls build-python/bin/cpp-native-solver
RCSPP_BENCHMARK_BUILD=build-python pytest -rs -q benchmarks/column_generation/tests | tail -5
```

Expected: every test passes (421 on the laptop), skips only for a missing piece. If the build runs out of memory,
again with `--parallel 1`. Paste the tail if anything fails.

## Step 2: the three files, checked

```bash
cd ~/rcspp/benchmarks/column_generation
cat > post-rebase-legs.toml <<'EOF'
# After the rebase onto the kernel stack: layers-50-legs on five instances, to compare run by run.
[benchmark]
repeats = 3
timeout = 3600
[defaults]
start = { type = "artificial" }
column_generation = { threads = 1 }
[matrix]
instance = ["C101_50", "C201_50", "R101_50", "R201_50", "RC101_50"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["exact"]
EOF
cat > post-rebase-exact.toml <<'EOF'
# After the rebase onto the kernel stack: t120-exact on six instances, to compare run by run.
[benchmark]
repeats = 3
timeout = 900

[defaults]
start = "single-customer"
column_generation = { threads = 1, timeout_s = 600 }

[matrix]
instance = ["C101_25", "R101_25", "C101_50", "R101_50", "RC101_50", "R101"]
layer = ["package", "cpp-bindings", "cpp-native"]
master = ["highspy", "gurobipy", "highs", "gurobi"]
pricing = ["exact"]
EOF
cat > post-rebase-tabu-50.toml <<'EOF'
# After the rebase onto the kernel stack: t120-tabu-50 as it was, to compare run by run.
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
for f in post-rebase-legs post-rebase-exact post-rebase-tabu-50; do
  python benchmark.py $f.toml --machine home-server --check options | grep -v dropped | head -1
done                                   # 35, 42 and 14 runs, 3 rounds
```

## Step 3: the chain

```bash
cat > ~/run-post-rebase.sh <<'EOF'
#!/usr/bin/env bash
cd ~/rcspp/benchmarks/column_generation && source ~/rcspp/.venv/bin/activate
for f in post-rebase-exact post-rebase-tabu-50 post-rebase-legs; do
  python benchmark.py $f.toml --processes 1 --machine home-server
done
echo "done $(date -u)"
EOF
chmod +x ~/run-post-rebase.sh
tmux new -s post "~/run-post-rebase.sh 2>&1 | tee ~/run-post-rebase.log; exec bash"
# Ctrl-b d to leave; tmux attach -t post to look; the same script again resumes a stopped benchmark;
# touch results/<file>/STOP to stop one cleanly
```

The short ones first, so a first answer comes within 15 min. Progress: `cat results/post-rebase-legs/status.txt`.

## Step 4: results back and the comparison (laptop, Git Bash)

```bash
cd /c/dev/rcspp/benchmark-results/colgen/home-server
scp -r 'leo@<server>:~/rcspp/benchmarks/column_generation/results/{post-rebase-legs,post-rebase-exact,post-rebase-tabu-50}' .
```

Then, per run, new against old, from the results folder (Git Bash turns an absolute `folder:run` into a Windows
path list, so the names stay relative):

```bash
C=C:/dev/rcspp-dev/benchmarks/column_generation/compare.py
for pair in post-rebase-legs:layers-50-legs post-rebase-exact:t120-exact post-rebase-tabu-50:t120-tabu-50; do
  new=${pair%%:*}; old=${pair##*:}
  for d in $new/runs/*-r1; do
    n=$(basename "$d" -r1)
    echo "### $new vs $old: $n"
    PYTHONIOENCODING=utf-8 python $C "$new:$n" "$old:$n" --time-within 5%
  done
done > post-rebase-comparison.txt
```

What to read: objective equal everywhere (a difference is a bug); iterations and columns equal on exact pricing
(a difference: ties broken otherwise, look before calling it fine); S11's tabu cells may differ (capped searches stop
elsewhere now); times within the server's 1-3 % spread, or a kernel speed change to report to the colleagues.

## Log

| date | what |
|---|---|
| 2026-11-24 | written; run on the server (126, 42, 105 runs finished, 0 failed), copied back |
| 2026-11-24 | compared: all 91 runs equal in objective, iterations, columns; times -2.0 % at the median (`F-106`) |
