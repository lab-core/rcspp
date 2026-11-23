"""The chart's data: per instance and master, the pure layer's extra time per iteration
against cpp-bindings.

  python make_data.py

Reads ../../home-server/t120-exact/summary.csv, writes highspy.dat and gurobipy.dat.
"""

import csv
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SUMMARY = os.path.join(HERE, "..", "..", "home-server", "t120-exact", "summary.csv")

with open(SUMMARY, encoding="utf-8") as file:
    rows = list(csv.DictReader(file))

cells = {(row["instance"], row["layer"], row["master"]): row for row in rows}

for master in ("highspy", "gurobipy"):
    lines = ["instance bindings_ms extra_ms extra_percent"]

    for (instance, layer, row_master), pure in sorted(cells.items()):
        if layer != "package" or row_master != master:
            continue

        bindings = cells[(instance, "cpp-bindings", master)]
        assert pure["iterations"] == bindings["iterations"], instance

        iterations = int(pure["iterations"])
        pure_seconds = float(pure["solve_seconds_median"])
        bindings_seconds = float(bindings["solve_seconds_median"])
        bindings_ms = 1e3 * bindings_seconds / iterations
        extra_ms = 1e3 * (pure_seconds - bindings_seconds) / iterations
        extra_percent = 100 * (pure_seconds - bindings_seconds) / bindings_seconds
        # LaTeX reads the names as text: escape the underscores.
        name = instance.replace("_", "\\_")
        lines.append(f"{name} {bindings_ms:.3f} {extra_ms:.3f} {extra_percent:.1f}")

    with open(os.path.join(HERE, f"{master}.dat"), "w", encoding="utf-8") as file:
        file.write("\n".join(lines) + "\n")

    print(f"{master}: {len(lines) - 1} cells")
