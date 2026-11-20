# Home server: setup to run the column generation benchmark

Everything needed to set up the home server from scratch and run the benchmark of
`feature/column-generation-benchmark` (PR #97) there, for long runs (`T-1.24`: the "gateway laptop" of `D-233`).
Updated as the steps are done. Runs and their analysis: `home-server-runs.md`.

## The machine

| item | value |
|---|---|
| OS | Ubuntu 24.04.4 LTS, x86_64, reached by ssh as an admin user |
| compilers | gcc and g++ 13.3.0 (system; C++23 and `<format>` as the column generation example needs) |
| git | 2.43.0 (system) |
| Python | 3.12.3 (system, unused: `uv` brings its own, with headers) |
| CPU, memory | 2 cores, 7.7 GiB, 4 GiB swap; governor `schedutil` (left as is: the benchmark records the frequency) |
| disk | `/` on LVM (`ubuntu--vg-ubuntu--lv`): 98 GB, 15 GB free (84 %); this setup takes about 4 GB |
| user | `leo`; clone at `/home/leo/rcspp` |
| other load | a small website, a small node server (likely Docker), cloudflared, Tailscale, a metrics script every 5 s, |
| | rare ssh sessions; idle: load 0.00, 95 % idle (2026-11-20, after removing a looping LXD container) |
| Gurobi | WLS licence, 2 sessions (probably the most at once: server + desktop), no version in it |

## Choices

- **Nothing global**: no `apt`, no `sudo`. Everything sits in `~/.local/bin` (`uv`, `git-lfs`), `~/opt` (Gurobi),
  `~/rcspp` (clone, `.venv`, `build-python`); `~/gurobi.lic`. Removing these uninstalls everything.
- Python 3.12 managed by `uv` (headers included, no `python3-dev` or `python3.12-venv` needed); CMake (< 4, as CI)
  and Ninja from PyPI in the venv.
- Gurobi **13.0.1**, as the desktop (`C:\gurobi1301`, `gurobipy` 13.0.1): same solver version on both machines.
- HiGHS: the build fetches and compiles v1.15.1 inside `build-python`.
- The build lives inside the clone (`build-python/`): the benchmark refuses a build configured from another tree.
- `-DCMAKE_CXX_CLANG_TIDY=`: without it CMake turns clang-tidy on (the benchmark README's command omits it).
- Runs: `--processes 1`, `column_generation.threads = 1` (2 cores, the website keeps one), `--machine home-server`,
  always inside `tmux` (closing ssh sends SIGHUP, which interrupts the benchmark; a resume picks it up again).
- Timings from this machine compare only with each other, not with the desktop's.

## Risks

- Gurobi's C++ library on Linux was never built by this repo before 2026-11-19 (CI has no licence, the Docker
  checks used HiGHS); it built here (step 5). Should it fail after an update, build without
  `USE_COLUMN_GENERATION_EXAMPLE_GUROBI`: `gurobipy` still works.
- Step 6 runs the benchmark's POSIX signal paths, never run on Linux before: they passed 2026-11-19 (at `b6edcb20`).
- WLS at 2 sessions: a third machine using the licence at the same time makes Gurobi runs fail (recorded as
  crashed; `--retry-crashes` runs them again).
- 2 jobs at once may run short of memory on the heavy template files: build again with `--parallel 1`.

## Steps

Every command runs on the server, in its ssh session. Paste the output back after each step.

| step | what | status |
|---|---|---|
| 0 | checks | done 2026-11-19 |
| 1 | local tools: `uv`, `git-lfs` | done 2026-11-19: uv 0.13.9, git-lfs 3.6.1 |
| 2 | clone, submodule, instances | done 2026-11-19: `b6edcb20`, instances pulled, pybind11 checked out |
| 3 | Python and the venv | done 2026-11-19: Python 3.12.16, CMake 3.31.10, Ninja 1.13.2; `highspy` pinned after |
| 4 | Gurobi and its licence | done 2026-11-19 (the user's word; output not pasted) |
| 5 | build | done 2026-11-19: Gurobi C++ found and linked, HiGHS 1.15.1 fetched, pybind11 3.0.1; 275 steps |
| 6 | the benchmark's tests | done 2026-11-19: 389 passed, 1 skipped (Windows drive letters); POSIX signals pass |
| 7 | `example.toml` once | done 2026-11-20: 44 / 44 finished, checks passed, layers equal per master |
| 8 | a long run under `tmux`, results back to the desktop | running: `layers-50`, 132 / 225 at 14:09 UTC 11-20 |

### 0. Checks

```bash
uname -m                              # x86_64
which curl tar                        # both present
echo "$PATH" | tr ':' '\n' | grep -x "$HOME/.local/bin"   # empty is fine: step 1 adds it
df -h ~                               # a few GB free (HiGHS build, Gurobi ~1.5 GB unpacked)
```

### 1. Local tools

`uv` (installs into `~/.local/bin` and adds it to the shell's PATH):

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
source ~/.local/bin/env               # or log out and in again
uv --version
```

`git-lfs`, the binary only:

```bash
mkdir -p ~/.local/bin /tmp/git-lfs && cd /tmp/git-lfs
curl -LO https://github.com/git-lfs/git-lfs/releases/download/v3.6.1/git-lfs-linux-amd64-v3.6.1.tar.gz
tar xzf git-lfs-linux-amd64-v3.6.1.tar.gz
cp git-lfs-3.6.1/git-lfs ~/.local/bin/
cd ~ && rm -rf /tmp/git-lfs
git lfs version
```

### 2. Clone

```bash
cd ~
git clone --branch feature/column-generation-benchmark --recurse-submodules https://github.com/lab-core/rcspp.git
cd ~/rcspp
git lfs install --local               # the LFS filter for this clone only
git lfs pull
git log --oneline -1
head -3 instances/C101_25.txt         # instance text, not "version https://git-lfs..."
ls extern/pybind11/CMakeLists.txt
```

### 3. Python and the venv

```bash
cd ~/rcspp
uv python install 3.12
uv venv --python 3.12 --python-preference only-managed .venv
source .venv/bin/activate
uv pip install -r benchmarks/column_generation/requirements.txt "cmake>=3.26,<4" ninja gurobipy==13.0.1 \
    highspy==1.15.1
python --version && cmake --version && ninja --version
```

`.venv/` is ignored by git. `uv venv` has no `pip`: install with `uv pip`. `highspy` is pinned to 1.15.1: the HiGHS
the build fetches for the C++ example and the desktop's `highspy`; unpinned, `uv` takes the newest (1.16.0 on
2026-11-19), and the two layers would run two HiGHS versions.

### 4. Gurobi and its licence

The C++ library, for the example's C++ Gurobi master: **optional**, the benchmark does not use it (step 5):

```bash
mkdir -p ~/opt && cd ~/opt
curl -LO https://packages.gurobi.com/13.0/gurobi13.0.1_linux64.tar.gz
tar xzf gurobi13.0.1_linux64.tar.gz && rm gurobi13.0.1_linux64.tar.gz
ls ~/opt/gurobi1301/linux64/lib       # libgurobi130.so, libgurobi_c++.a
```

Gurobi found from the venv only (appended to its activate script):

```bash
cat >> ~/rcspp/.venv/bin/activate <<'EOF'
export GUROBI_HOME="$HOME/opt/gurobi1301/linux64"
export LD_LIBRARY_PATH="$GUROBI_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
EOF
```

The WLS licence: an API key from Gurobi's Web License Manager (its secret is shown only when the key is created;
lost, make a new key). `~/gurobi.lic` (where Gurobi looks by default) holds three lines, written on the server, never
pasted in a chat:

```text
WLSACCESSID=<access id>
WLSSECRET=<secret>
LICENSEID=<licence number>
```

```bash
nano ~/gurobi.lic && chmod 600 ~/gurobi.lic
```

Then:

```bash
cd ~/rcspp && source .venv/bin/activate
python -c "import gurobipy as gp; m = gp.Model(); print(gp.gurobi.version())"   # WLS licence lines, (13, 0, 1)
```

### 5. Build (in `tmux`: 30 to 60 min the first time, HiGHS included)

```bash
tmux new -s build                     # detach: Ctrl-b then d; back: tmux attach -t build
cd ~/rcspp && source .venv/bin/activate
echo "$GUROBI_HOME"                   # /home/leo/opt/gurobi1301/linux64
cmake -B build-python -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DUSE_PYTHON=ON -DUSE_TESTS=OFF \
    -DUSE_COLUMN_GENERATION_EXAMPLE=ON \
    -DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON \
    -DUSE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON \
    -DPython_EXECUTABLE="$PWD/.venv/bin/python" \
    "-DCMAKE_CXX_CLANG_TIDY="
cmake --build build-python --parallel 2 2>&1 | tee build-python.log
cmake --build build-python            # "ninja: no work to do."
```

`_core` does not link Gurobi (`ldd` shows none): the bindings have no C++ Gurobi master; both layers reach Gurobi
through `gurobipy` (`runners/cpp_bindings_runner.py`, masters `HighsMasterSolver`, `HighspyMasterSolver`,
`GurobipyMasterSolver`). `USE_COLUMN_GENERATION_EXAMPLE_GUROBI` builds only the example's C++ Gurobi master, used by
its C++ tests (off here): the benchmark needs neither it nor `~/opt/gurobi1301` (2 GB) nor `GUROBI_HOME`. Kept for
now; to drop them, configure without the option, delete the folder and the two lines in `.venv/bin/activate`.

`build-python.log` is in the clone's root but untracked: the benchmark's record of uncommitted changes ignores
untracked files.

### 6. The benchmark's tests

```bash
cd ~/rcspp && source .venv/bin/activate
RCSPP_BENCHMARK_BUILD=build-python pytest -rs benchmarks/column_generation/tests
```

Expected: everything passes; skips listed by `-rs` are expected only for a missing piece.

### 7. `example.toml` once

22 runs, 2 rounds (about 1.5 min on the desktop; slower here):

```bash
cd ~/rcspp/benchmarks/column_generation && source ~/rcspp/.venv/bin/activate
python benchmark.py example.toml --machine home-server
cat results/example/summary.md
```

Results of 2026-11-20 against the desktop's `results/example` (same commit, versions):

| | desktop (i7-1165G7) | server (Celeron 2950M, 2 GHz) |
|---|---|---|
| C101_25 / R201_25, `highspy` and `highs`, exact | 16 / 599, 16 / 4185 | the same |
| C101_25 / R201_25, `gurobipy`, exact | 23 / 691, 16 / 4465 | 21 / 694, 19 / 4476 (same objectives) |
| R201_25 `highspy` exact, solve | 0.47 s | 1.42 s (2-3 times slower overall) |
| `other CPU %` | about 20 % of 8 threads | about 55 % of 2 cores (to check on an idle server) |

Gurobi's iterations differ between machines (its default LP method races on every thread; likely cause): compare
Gurobi runs within one machine; fixing `Method` and `Threads` may make them equal, not checked.

### 8. Long runs

First long run (2026-11-20): the layer comparison on the 50-customer instances, while `cpp-native` is waiting; a
baseline on this machine for it. 15 instances × 5 layer-master pairs (`package` has no `highs`) × 3 rounds = 225
runs, about 2.5-4 h (type 1 a few s each, type 2 1 min or more; R202_50 not yet measured). The file is untracked on
the server (not in the record's digest of uncommitted changes):

```bash
cd ~/rcspp/benchmarks/column_generation
cat > layers-50.toml <<'EOF'
# The two layers on every 50-customer instance, each master, exact pricing, 3 rounds.
[benchmark]
repeats = 3
timeout = 3600

[defaults]
start = { type = "artificial" }
column_generation = { threads = 1 }

[matrix]
instance = ["C101_50", "C102_50", "C103_50", "C201_50", "C202_50",
            "R101_50", "R102_50", "R103_50", "R105_50", "R107_50", "R201_50", "R202_50",
            "RC101_50", "RC102_50", "RC201_50"]
layer = ["package", "cpp-bindings"]
master = ["highspy", "gurobipy", "highs"]
pricing = ["exact"]
EOF
source ~/rcspp/.venv/bin/activate
python benchmark.py layers-50.toml --machine home-server --check options   # lists 75 runs, runs nothing
```

`timeout = 3600` only guards against a run that never ends; no `column_generation.timeout_s`, so every run can end
`Optimal` and the comparison's time rule can judge it. Afterwards: `summary.md`, then `compare.py` per instance and
master with `--time-within 5%`.

The general form:

```bash
tmux new -s bench
cd ~/rcspp/benchmarks/column_generation && source ~/rcspp/.venv/bin/activate
python benchmark.py <file>.toml --processes 1 --machine home-server
# Ctrl-b d to leave; tmux attach -t bench to come back; touch results/<file>/STOP to stop cleanly;
# the same command again resumes.
```

Results back to the desktop (from the desktop):
`scp -r leo@<server>:~/rcspp/benchmarks/column_generation/results/<file> .`

## Updating

```bash
cd ~/rcspp && source .venv/bin/activate
git pull && git lfs pull && git submodule update --init --recursive
cmake --build build-python --parallel 2  # copies python/src again; the benchmark refuses stale copies
```

A benchmark folder started on another commit refuses to resume unless `--allow-mixed`.

## Log

| date | what |
|---|---|
| 2026-11-19 | steps 0-6 done (statuses above); `highspy` came as 1.16.0, pinned to 1.15.1 |
| 2026-11-19 | disk: `/home` 59 GB (50 GB of the user's GTFS data, removable), `/var` 13 GB (Docker, LXD), `/usr` 3.4 GB |
| 2026-11-20 | step 7 done; idle check: one core at 100 % in a `snapd` restart loop (below) |
| 2026-11-20 | **incident:** an LXD container `recover`, left from the user's password recovery (its `lxc delete -f` |
| | had not taken), privileged and mounting the host's root, its `snapd` failing under AppArmor every ~4 min. |
| | Checked (shell history, groups, logins, ssh keys: the user's own), deleted: load 0.00, 95 % idle after |
| | (step 7's times ran with one core taken) |
| 2026-11-20 | R201_50 alone: 33 iterations, 64.6 s; `layers-50` started 05:10 UTC (`home-server-runs.md`) |
| 2026-11-20 | #97 moved to `ee8b0272` (the server's findings fixed): the server stays on `b6edcb20` until `layers-50` ends (a |
| | resume refuses another commit), then `Updating` above; its `highspy` is already 1.15.1 |
