"""The three layers per master family on one benchmark folder: equality, totals by D-139's rule, time per iteration.

  python analyze_legs.py <folder>
"""

import csv
import statistics
import sys

FAMILY = {"highspy": "HiGHS", "highs": "HiGHS", "gurobipy": "Gurobi", "gurobi": "Gurobi"}
# The pure layer against the C++ layers, each over the master it shares or the closest one.
PAIRS = {
    "HiGHS": (("package", "highspy"), [("cpp-bindings", "highspy"), ("cpp-bindings", "highs"), ("cpp-native", "highs")]),
    "Gurobi": (("package", "gurobipy"), [("cpp-bindings", "gurobipy"), ("cpp-native", "gurobi")]),
}


def main():
    folder = sys.argv[1]
    rows = list(csv.DictReader(open(f"{folder}/summary.csv")))
    pricing = sorted({r["run"].split("-")[-4] if "tabu" in r["run"] else "exact" for r in rows})
    cells = {}
    for r in rows:
        key = (r["instance"], FAMILY[r["master"]], "tabu" if "tabu" in r["run"] else "exact")
        cells.setdefault(key, {})[(r["layer"], r["master"])] = r
    unequal, verdicts = [], []
    print(f"# {folder}\n")
    print("| instance | pricing | master | it. | col. | ms/it C++ (best) | pure extra ms/it vs each C++ | pure extra % | "
          "solve s pure / C++ | D-139 |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for (instance, family, kind), runs in cells.items():
        signature = {(r["objective"][:12], r["iterations"], r["columns"]) for r in runs.values()}
        if len(signature) > 1:
            unequal.append((instance, family, kind, signature))
        pure_key, others = PAIRS[family]
        if pure_key not in runs:
            continue
        pure = runs[pure_key]
        it = int(pure["iterations"])
        pure_s = float(pure["solve_seconds_median"])
        extras, cpp_times = [], []
        for key in others:
            if key in runs:
                other_s = float(runs[key]["solve_seconds_median"])
                extras.append(1e3 * (pure_s - other_s) / int(runs[key]["iterations"]))
                cpp_times.append(other_s)
        best = min(cpp_times)
        best_per_it = 1e3 * best / it
        worst_gap = pure_s - best
        rule = "pass" if worst_gap <= max(0.05 * best, 0.25) else "FAIL"
        extra_pct = 100 * (pure_s - best) / best
        verdicts.append((instance, kind, family, best_per_it, max(extras), extra_pct, rule, pure_s, best))
        print(f"| {instance} | {kind} | {family} | {it} | {pure['columns']} | {best_per_it:.2f} | "
              f"{' / '.join(f'{e:+.2f}' for e in extras)} | {extra_pct:+.1f} | {pure_s:.2f} / {best:.2f} | {rule} |")
    print(f"\nUnequal cells (objective, iterations, columns across layers): {len(unequal)}")
    for u in unequal:
        print("  ", u)
    fails = [v for v in verdicts if v[6] == "FAIL"]
    print(f"D-139 failures (pure against the fastest C++ layer): {len(fails)} of {len(verdicts)}")
    spreads = [float(r["solve_seconds_spread_percent"]) for r in rows if r["solve_seconds_spread_percent"]]
    print(f"Spread of solve times: median {statistics.median(spreads):.1f} %, max {max(spreads):.1f} %")


if __name__ == "__main__":
    main()
