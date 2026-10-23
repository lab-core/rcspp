# Baselines

Golden baselines are recorded at the commit where implementation starts (`D-11`, `T-0.1`). Rows marked
_informative_ only show the order of magnitude.

## How to run (T-0.1b)

Windows, PowerShell 7, from the repo root. Needs `build/` configured with `USE_VRP=ON` and Gurobi found (else the
targets do not exist, `I-07`); `build/` uses the Visual Studio generator, hence `--config Release` and
`bin\Release`. A build regenerates the tracked `cpp/rcspp/rcspp.hpp` from every header under `cpp/rcspp/`, drafts
included.

### Targets

| target                      | instances                                                                                      | arguments (defaults)                                                                                                           | output                                                                                                                                                                                |
| --------------------------- | ---------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rcspp-vrp-benchmark`       | Solomon C1, R1, RC1 (100 customers): C10i, R10i, RC10i for i = 1…N                             | `N` (2, at most 9); `--astar` adds A\*; `--boost` adds Boost (needs Boost at configure)                                        | table; N = 1 took ~26 s on LEO-LAPTOP                                                                                                                                                 |
| `rcspp-vrp-benchmark-large` | Solomon C2 and RC2: C20i, RC20i for i = 1…N; R2: R201…R2M; Gehring & Homberger with `--gh-dir` | `N` (2, at most 8); `--r2-max M` (= N, at most 11); `--gh-dir <dir of .txt>` (instances not in the repo); `--astar`; `--boost` | table; long horizons, slower (not measured)                                                                                                                                           |
| `rcspp-ng-cg-comparison`    | any file of `instances/` by name without `.txt` (`C101_25` = first 25 customers of C101)       | `--ng-sizes=0,4,8` (0,8); instance names (`R101_25 C201_50 R201_25 RC201_50`)                                                  | per instance and ng size: LP bound, iterations, seconds; needs `kRouteRelaxation = RouteRelaxation::NgPath` in `examples/cpp/vrp/vrp.hpp` (`static_assert`), not in the default build |

The first two price with the relaxation set in `vrp.hpp` (`None` on `main`: cycles allowed, `F-05`, `I-06`).

### Run

Set the two variables, then paste the whole block. It writes
`benchmark-results\<yyyyMMdd-HHmm>-<commit>-<machine>\full.log` (the log, ANSI colors stripped, `I-04`) and
`summary.md` (machine, build and the final table).

````powershell
$target = "rcspp-vrp-benchmark"  # or rcspp-vrp-benchmark-large, rcspp-ng-cg-comparison
$benchmarkArgs = @("1")          # e.g. @("2", "--astar"), @("2", "--r2-max", "4"), @("--ng-sizes=0,8", "C101_25")

cmake --build build --config Release --target $target
$commit = git rev-parse --short HEAD
$dirty = if (git status --porcelain --untracked-files=no -- . ":(exclude)plans" ":(exclude)benchmark-results") { " (uncommitted changes)" } else { "" }
$out = "benchmark-results\$(Get-Date -Format yyyyMMdd-HHmm)-$commit-$env:COMPUTERNAME"
New-Item -ItemType Directory -Force $out | Out-Null
& ".\build\bin\Release\$target.exe" @benchmarkArgs 2>&1 |
  ForEach-Object { "$_" -replace "`e\[[0-9;]*m", "" } | Tee-Object "$out\full.log"

$cache = Get-Content build\CMakeCache.txt
$compiler = Get-ChildItem build\CMakeFiles\*\CMakeCXXCompiler.cmake | Sort-Object LastWriteTime | Select-Object -Last 1 |
  Select-String 'CMAKE_CXX_COMPILER_(ID|VERSION) "(.*)"' | ForEach-Object { $_.Matches[0].Groups[2].Value }
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
$os = Get-CimInstance Win32_OperatingSystem
$onBattery = (Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue).BatteryStatus -eq 1
$relaxation = (Select-String examples\cpp\vrp\vrp.hpp -Pattern 'kRouteRelaxation = RouteRelaxation::(\w+)').Matches[0].Groups[1].Value
$lines = Get-Content "$out\full.log"
$marks = @(($lines | Select-String -Pattern '^=+$').LineNumber)
$table = if ($marks.Count -ge 2) { $lines[$marks[0]..($marks[-1] - 2)] } else { $lines }
@("# $target", "",
  "- date: $(Get-Date -Format s)",
  "- commit: $commit$dirty",
  "- args: $($benchmarkArgs -join ' ')",
  "- machine: $env:COMPUTERNAME",
  "- cpu: $($cpu.Name.Trim()) ($($cpu.NumberOfCores) cores, $($cpu.NumberOfLogicalProcessors) threads)",
  "- memory: $([math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB)) GB",
  "- os: $($os.Caption) $($os.Version)",
  "- power: $(if ($onBattery) { 'on battery (slower timings)' } else { 'plugged in or desktop' })",
  "- build: Release, $(($cache -match '^CMAKE_GENERATOR:') -replace '^.*=', ''), $($compiler -join ' ')",
  "- gurobi: $(($cache -match '^GUROBI_LIBRARY:') -replace '^.*=', '')",
  "- route relaxation: $relaxation",
  "", '```text') + $table + '```' | Set-Content "$out\summary.md"
````

- The summary's table is the log between the first and the last `====` line; the ng comparison has none, so its
  whole log is copied.
- "Uncommitted changes" ignores `plans/` and `benchmark-results/`; a build that regenerated `rcspp.hpp` sets it.
- Record a new section below by hand (newest first), with the machine and build lines of `summary.md`.

### Reading the VRP benchmark table

- **One CG run per instance**, driven by the first algorithm (Simple): at every iteration, all seven algorithms
  price the same duals and are timed, but only Simple's columns enter the master (`examples/cpp/vrp/vrp.hpp`,
  `VRP::solve`). A column = that algorithm's total pricing time over the same dual sequence, not a CG run with it.
- The master LP is in no column. Time outside pricing ≈ wall time (log timestamps) − the sum of the seven columns.
- `Gap%` compares the LP bound with a BKS computed on 1-decimal distances (`F-11`): `FAIL` on C101 is false (#54).
- Per-iteration log lines (`nb_iter`, `obj`, `min_reduced_cost`, `paths_generated`) allow exact run comparisons.

## 2026-10-23 · reference · `129b15e0` (`main`) · LEO-LAPTOP

Run: `rcspp-vrp-benchmark 1`. i7-1165G7 (4 cores, 8 threads), 12 GB, Windows 11 Home, plugged in. Release,
Visual Studio 18 2026, MSVC 19.51.36260.0, Gurobi 13.0.1. Route relaxation `None` (cycles allowed, `F-05`). Tree:
`129b15e0` plus the draft column generation headers pulled into `rcspp.hpp` (compiled, not used by the benchmark).
Raw output: `benchmark-results/20261023-1045-129b15e0-LEO-LAPTOP/`.

| instance | LP      | BKS (1-dec.) | gap                    | LP solves | stalls | columns | wall (s) | Simple (s) | Pushing (s) | Pulling (s) | ConstrTabu (s) | Tabu (s) | BucketS (s) | BucketP (s) |
| -------- | ------- | ------------ | ---------------------- | --------- | ------ | ------- | -------- | ---------- | ----------- | ----------- | -------------- | -------- | ----------- | ----------- |
| C101     | 828.94  | 827.3        | +0.20 % (false `FAIL`) | 87        | 39     | 3668    | 7.84     | 1.14       | 1.03        | 0.97        | 0.89           | 1.25     | 1.09        | 1.09        |
| R101     | 1636.39 | 1637.7       | −0.08 %                | 33        | 6      | 1581    | 2.20     | 0.33       | 0.27        | 0.26        | 0.25           | 0.38     | 0.32        | 0.31        |
| RC101    | 1572.74 | 1619.8       | −2.91 %                | 33        | 0      | 2449    | 6.06     | 0.84       | 0.75        | 0.68        | 0.45           | 1.17     | 1.01        | 1.00        |
| total    |         |              |                        | 153       | 45     | 7698    | 16.1     | 2.30       | 2.04        | 1.91        | 1.59           | 2.80     | 2.41        | 2.40        |

Stalls = LP solves whose objective equals the previous one (as printed). Columns = sum of `paths_generated`.

Findings:

- **Reproducible.** LP values, LP solves and every per-iteration line (objective, minimum reduced cost to 20
  digits, columns) are identical to the 2026-10-20 run on `fdc41b26`, over all 153 iterations. The bidirectional
  merge left default pricing unchanged; these LP values are regression targets for relaxation `None` (`T-0.1`
  still needs `Q-09` and its own instance list).
- **Times are 34–43 % lower than on 2026-10-20**, by the same factor (1.57–1.75×) for all seven algorithms. A
  uniform gain points to the machine or compiler, not the code; that run's machine was not recorded (another
  checkout, `C:\Users\marbe\Desktop\…`). Compare times only between runs on the same machine.
- **Algorithms.** Among exact ones, Pulling is fastest (1.91 s), then Pushing (2.04) and Simple (2.30); buckets
  (2.40) do not beat plain lists here. ConstructiveTabu (1.59) and Tabu (2.80) are heuristics.
- **Degeneracy.** C101 needs 87 LP solves against 33, and 39 of them leave the objective unchanged (`F-13`): it
  takes half the run. Evidence for stabilization (T-2.4).
- **The master matters.** Time outside pricing is 0.38 s (C101), 0.08 (R101), 0.16 (RC101): master LP, dual
  updates, column adds, logging. With a single pricer (Simple), that is 25 %, 20 % and 16 % of the run: the master
  LP speed counts in the backend choice (`Q-14`).
