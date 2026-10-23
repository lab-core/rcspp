# fix(benchmark): the BKS table uses 1-decimal distances, so correct results are flagged FAIL

## Summary

`rcspp-vrp-benchmark` flags a result `FAIL` when the LP value exceeds the best known solution (BKS) from `kSolomonBKS` (`examples/cpp/benchmark_common.hpp`, check at line ~126, with `kBKSTolerance = 1e-3`). On 2026-10-20 (`main` @ `fdc41b26`, `rcspp-vrp-benchmark 1`), **C101 printed `828.94  +0.20% FAIL` against a BKS of 827.3**. The result is correct; the comparison is not.

## Root cause

- The VRP example uses **unrounded Euclidean distances**, and travel time = distance (`VRP::calculate_distance`: `std::sqrt(...)`, no rounding).
- The BKS table follows the convention of most exact-method papers on Solomon's instances: **distances truncated to one decimal**. C101's optimum is 827.3 under that convention and 828.94 with unrounded distances. C101 has no integrality gap, so its LP reaches 828.94 and exceeds the table.
- More generally, the check assumes LP ≤ BKS. That holds only if the BKS uses the same distances. Under different conventions, any instance with a tight relaxation (the C1 family in particular) can be flagged.
- There is **no simple safe tolerance**. Truncated travel times are shorter, so a route that meets its time windows with truncated times may miss them with real times. A truncated-distance solution isn't necessarily feasible under real distances, so its cost plus a slack is not a valid bound.
- The table's provenance is "Solomon (1987) and subsequent improvements": the source of each entry should be checked. Some published BKS minimize the number of vehicles first, so they aren't distance-only optima.

## Options

| option | change | effect |
|---|---|---|
| A (minimal) | rename the column "BKS (1-decimal distances)", keep the gap, print `n/c` (not comparable) instead of `FAIL` | no false alarms; no check either |
| B (recommended, with A) | add a distance convention to the VRP example (`DistanceRounding::{None, Truncate1}`), applied to both distance and travel time; under `Truncate1`, keep the strict `FAIL` check | a valid regression check, and results comparable with published optima and LP bounds |
| C | replace the table with optima for unrounded distances | fewer published sources; more work |

## Done when

C101 no longer reports `FAIL` by default. With `Truncate1`, every LP value is ≤ its BKS, and a violation is a real `FAIL`.

Checked on `main` @ `129b15e0`.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
