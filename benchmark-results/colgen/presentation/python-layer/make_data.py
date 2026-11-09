"""Data files for slides.tex (pure-Python layer against C++-backed), from the full-*/ and rerun-*/ runs."""
import csv, json, os, statistics
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
COLGEN = os.path.dirname(os.path.dirname(HERE))
TIERS = {  # folder: start; rerun-small-single replaces full-small-single (quieter machine)
    "rerun-small-single": "single", "full-small-bigm": "bigm", "full-mid-single": "single",
    "full-mid-bigm": "bigm", "full-long-single": "single", "full-long-bigm-tabu": "bigm",
}
runs = 0
EOL = r" \\" + "\n"
END_PCT = r"\,\% & "
cells = defaultdict(lambda: defaultdict(list))
paths = defaultdict(set)
iterations = {}
for folder, start in TIERS.items():
    with open(os.path.join(COLGEN, folder, "results.csv")) as f:
        for r in csv.DictReader(f):
            runs += 1
            _, layer, master, pricing = r["config"].split("-")
            key = (folder, r["instance"], master, pricing, start)
            cells[key][layer].append(float(r["solve_s"]))
            paths[key].add((r["lp"][:12], r["iterations"], r["columns"]))
            iterations[key] = int(r["iterations"])
rows = []
for key, t in cells.items():
    c, p = statistics.median(t["cpp"]), statistics.median(t["purepython"])
    spread = max(100 * (max(t[layer]) - min(t[layer])) / statistics.median(t[layer]) for layer in t)
    rows.append((key, c, p, p - c, 100 * (p - c) / c, len(t["cpp"]), spread))
rows.sort(key=lambda row: row[1])
with open(os.path.join(HERE, "cells.dat"), "w") as f:
    f.write("C P diff pct pricing\n")
    for key, c, p, d, pct, n, _ in rows:
        f.write(f"{c:.6f} {p:.6f} {d:.6f} {pct:.4f} {key[3]}{'' if key[4] == 'single' else 'bigm'}\n")
with open(os.path.join(HERE, "long.tex"), "w") as f:
    for key, c, p, d, pct, n, _ in rows:
        if c > 30:
            f.write(f"{key[1]} & {key[2]} & {key[3]} & {key[4]} & {c:.1f} & {p:.1f} & {d:+.2f} & {pct:+.1f}" + END_PCT + f"{n}" + EOL)
# Cells over 100 s against their noise: spread = fastest to slowest repeat of one layer, in % of its median (the larger of the
# two layers); expected = the pure layer's measured extra cost per iteration (at most 5 ms) times the iterations.
with open(os.path.join(HERE, "noise.dat"), "w") as f:  # comma-separated: the labels have spaces
    f.write("index,label,pct,spread,repeats,expected\n")
    long_rows = [r for r in rows if r[1] > 100]
    for index, (key, c, p, d, pct, n, spread) in enumerate(reversed(long_rows)):
        start = "big-M" if key[4] == "bigm" else "single"
        label = f"{key[1]} {key[2]} {key[3]} {start} ({c:.0f} s)"
        f.write(f"{index},{label},{pct:.3f},{spread:.3f},{n},{100 * 0.005 * iterations[key] / c:.3f}\n")
buckets = [(0, 1), (1, 10), (10, 100), (100, 1e9)]
with open(os.path.join(HERE, "buckets.tex"), "w") as f:
    for lo, hi in buckets:
        b = [r for r in rows if lo <= r[1] < hi]
        pcts = [r[4] for r in b]
        label = f"{lo}--{hi} s" if hi < 1e9 else f"over {lo} s"
        f.write(f"{label} & {len(b)} & {min(pcts):+.1f} to {max(pcts):+.1f} & {statistics.median(pcts):+.1f} & "
                f"{max(r[3] for r in b):+.3f} & {sum(r[3] < 0 for r in b)}" + EOL)
for name in ("exact", "tabu"):
    with open(os.path.join(COLGEN, "full-probes", f"C201-{name}.txt")) as f:
        m = json.loads(next(l for l in f if l.startswith("PROBE "))[6:])["medians"]
    c, p = m["cpp"], m["pure"]
    print(name, {k: round(1e3 * (p[k] - c[k]), 1) for k in ("total", "master", "pricing", "rest")},
          {k: round(1e3 * p[k], 1) for k in ("add_columns", "to_arrays", "price_python", "loop_price", "to_result")},
          "C", round(c["total"], 2), "P", round(p["total"], 2))
print("runs", runs, "cells", len(rows), "same path everywhere", all(len(v) == 1 for v in paths.values()))
print("not equivalent", [r for r in rows if r[3] > max(0.05 * r[1], 0.25)])
