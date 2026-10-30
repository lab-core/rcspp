# Builds what is missing, then starts the runner. Arguments go to run_matrix.py:
#   .\run_matrix.ps1 --output benchmark-results/colgen/small --instances C101_25 --configs example-mip
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $root
$build = Join-Path $root "build-python-highs"
if (-not (Test-Path (Join-Path $build "python\rcspp\_core.cp314-win_amd64.pyd"))) {
    cmake --build $build --config Release --target _core
    if ($LASTEXITCODE -ne 0) { throw "build of _core failed" }
}

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

$env:PYTHONPATH = Join-Path $build "python"
& (Join-Path $root ".venv\Scripts\python.exe") (Join-Path $PSScriptRoot "run_matrix.py") @args
exit $LASTEXITCODE
