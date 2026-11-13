# Builds what is missing, then starts the runner. Arguments go to run_matrix.py:
#   .\run_matrix.ps1 --output benchmark-results/colgen/small --instances C101_25 --configs example-mip
#
# The rcspp code comes from a source checkout (the folded stack: the pure-Python package, the C++ column generation
# example, the VRP example, the instances), given by COLGEN_SOURCE_DIRECTORY and defaulting to ..\rcspp-dev next to this
# repository. `_core`, with the example's bindings, is built there in build-python-highs. Results stay in this repository.
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root
$python = Join-Path $root ".venv\Scripts\python.exe"

$source = $env:COLGEN_SOURCE_DIRECTORY
if (-not $source) { $source = Join-Path $root "..\rcspp-dev" }
if (-not (Test-Path (Join-Path $source "CMakeLists.txt"))) { throw "no rcspp source checkout at $source (set COLGEN_SOURCE_DIRECTORY)" }
$source = (Resolve-Path $source).Path
$build = Join-Path $source "build-python-highs"
$example = Join-Path $source "examples\column_generation"
$suffix = & $python -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))"
$core = Join-Path $build "python\rcspp\_core$suffix"

# Configured with the example and its HiGHS master solver (and its Gurobi one when Gurobi is installed); the cache tells
# whether the folder was configured so.
$cache = Join-Path $build "CMakeCache.txt"
$configured = (Test-Path $cache) -and (Select-String -Path $cache -Quiet -Pattern "^USE_COLUMN_GENERATION_EXAMPLE_HIGHS:BOOL=ON")
if (-not $configured) {
    $options = @(
        "-DUSE_PYTHON=ON", "-DUSE_TESTS=OFF", "-DUSE_COLUMN_GENERATION_EXAMPLE=ON",
        "-DUSE_COLUMN_GENERATION_EXAMPLE_HIGHS=ON", "-DPython_EXECUTABLE=$python"
    )
    if ($env:GUROBI_HOME -and (Test-Path $env:GUROBI_HOME)) { $options += "-DUSE_COLUMN_GENERATION_EXAMPLE_GUROBI=ON" }
    cmake -S $source -B $build @options
    if ($LASTEXITCODE -ne 0) { throw "configure of the source checkout failed" }
}
if (-not (Test-Path $core)) {
    cmake --build $build --config Release --target _core
    if ($LASTEXITCODE -ne 0) { throw "build of _core failed" }
}

# The build folder holds a copy of the source's python/src/rcspp next to `_core`; refresh it (a no-op when current), so
# that the pure layer run is the source's code as it is now.
cmake --build $build --config Release --target copy_python_sources
if ($LASTEXITCODE -ne 0) { throw "copy of the Python sources failed" }

# v2: the current C++ example, from a worktree of main (cycles allowed: ng size 0, which needs kRouteRelaxation = NgPath).
# Built only when the run includes example-cpp and the executable is missing.
$wantsCpp = ($args -notcontains "--configs") -or (($args -join " ") -match "example-cpp")
$main = Join-Path $root "..\rcspp-main"
$v2 = Join-Path $main "build-v2"
if ($wantsCpp -and -not (Get-ChildItem $v2 -Recurse -Filter rcspp-ng-cg-comparison.exe -ErrorAction SilentlyContinue)) {
    if (-not (Test-Path $main)) { git worktree add $main main; if ($LASTEXITCODE -ne 0) { throw "worktree failed" } }
    $header = Join-Path $main "examples\cpp\vrp\vrp.hpp"
    (Get-Content $header -Raw) -replace "kRouteRelaxation = RouteRelaxation::None", "kRouteRelaxation = RouteRelaxation::NgPath" | Set-Content $header -NoNewline
    cmake -S $main -B $v2 -DUSE_VRP=ON
    if ($LASTEXITCODE -ne 0) { throw "configure of the main worktree failed" }
    cmake --build $v2 --config Release --target rcspp-ng-cg-comparison
    if ($LASTEXITCODE -ne 0) { throw "build of rcspp-ng-cg-comparison failed" }
}

# The build's python folder first: `rcspp` is then the build's package, its compiled `_core` (with
# rcspp._core.cpp_column_generation) and its refreshed copy of the source's Python files (including the pure layer
# rcspp.column_generation). With the source's python/src first, `_core` would not be found. Then the source's python/src
# and the example's python folder (cpp_column_generation, the loop script). drivers.py adds the VRP example and the
# instances from the same source.
$env:COLGEN_SOURCE_DIRECTORY = $source
$env:PYTHONPATH = (Join-Path $build "python"), (Join-Path $source "python\src"), (Join-Path $example "python") -join ";"
& $python (Join-Path $PSScriptRoot "run_matrix.py") @args
exit $LASTEXITCODE
