"""The verdict of the rule that decides whether the C++ layer stays, per cell, from the full comparison folders.

  full_comparison_verdict.py [--floor 0.25] [folders...]   (default: every benchmark-results/colgen/full-*/)
"""

import argparse
import csv
import glob
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--floor", type=float, default=0.25)
    parser.add_argument("folders", nargs="*")
    args = parser.parse_args()
    folders = args.folders or sorted(glob.glob(os.path.join(HERE, "full-*", "")))
    failing, cells = [], 0
    print("| tier | instance | master | pricing | C++-backed s | pure s | difference | % | spread C | same path | verdict |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for folder in folders:
        path = os.path.join(folder, "results.csv")
        if not os.path.exists(path):
            continue
        tier = os.path.basename(os.path.normpath(folder)).removeprefix("full-")
        groups = {}
        for row in csv.DictReader(open(path)):
            if row["exit_code"] != "0":
                continue
            _, loop, master, pricing = row["config"].split("-")
            groups.setdefault((row["instance"], master, pricing), {}).setdefault(loop, []).append(row)
        for (instance, master, pricing), loops in groups.items():
            if set(loops) != {"cpp", "purepython"}:
                continue
            cells += 1
            c_rows, p_rows = loops["cpp"], loops["purepython"]
            c = statistics.median(float(r["solve_s"]) for r in c_rows)
            p = statistics.median(float(r["solve_s"]) for r in p_rows)
            lps = [float(r["lp"]) for r in c_rows + p_rows]
            gate = max(lps) - min(lps) <= 1e-6 * max(1.0, abs(max(lps))) and all(
                r["status"] == "Optimal" for r in c_rows + p_rows)
            same = {(r["iterations"], r["columns"]) for r in c_rows} == {(r["iterations"], r["columns"]) for r in p_rows}
            spread = max(float(r["solve_s"]) for r in c_rows) - min(float(r["solve_s"]) for r in c_rows)
            ok = p - c <= max(0.05 * c, args.floor)
            verdict = "LP DIFFERS" if not gate else ("equivalent" if ok else "FAILS")
            if verdict != "equivalent":
                failing.append((tier, instance, master, pricing, verdict))
            print(f"| {tier} | {instance} | {master} | {pricing} | {c:.3f} | {p:.3f} | {p - c:+.3f} | "
                  f"{100 * (p / c - 1):+.1f} | {spread:.3f} | {'yes' if same else 'NO'} | {verdict} |")
    print(f"\n{cells} cells, floor {args.floor} s; not equivalent: {len(failing)}")
    for f in failing:
        print("  ", *f)


if __name__ == "__main__":
    main()
