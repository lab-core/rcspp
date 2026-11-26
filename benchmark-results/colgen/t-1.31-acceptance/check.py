"""T-1.31's acceptance check over results/runs: every run Optimal; per instance and
relaxation, every LP equals the package's plain forward simple search's; none <= ng 8, and
on C101_10 ng 10 = elementary. Prints the failures, then the solve times per variant.

    python check.py [results folder]
"""

import json
import statistics
import sys
import tomllib
from collections import defaultdict
from pathlib import Path

TOLERANCE = 1e-6


def relaxation(run):
    graph = run.get("graph", {})
    name = graph.get("relaxation", "none")
    return f"ng {graph['ng_size']}" if name == "ng" else name


def search(run):
    stages = run.get("pricing", {}).get("stages", [{"algorithm": "simple"}])
    stage = stages[0]
    text = f"{stage['algorithm']} {stage.get('direction', 'forward')}"

    if "bucket_width" in stage:
        text += " buckets"

    if run.get("pricing", {}).get("preprocess") is False:
        text += " no-preprocess"

    return text


def main():
    folder = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).parent / "results")
    rows = []
    failures = []

    for run_folder in sorted((folder / "runs").iterdir()):
        result_path = run_folder / "result.json"

        if not result_path.exists():
            failures.append(f"{run_folder.name}: no result")
            continue

        run = tomllib.loads((run_folder / "run.toml").read_text(encoding="utf-8"))
        result = json.loads(result_path.read_text(encoding="utf-8"))
        instance = run["instance"]["name"] if isinstance(run["instance"], dict) else run["instance"]
        row = (instance, relaxation(run), run["layer"], search(run), result)
        rows.append(row)

        if result.get("status") != "Optimal":
            failures.append(f"{run_folder.name}: status {result.get('status')}")

    reference = {
        (instance, relaxed): result["objective"]
        for instance, relaxed, layer, searched, result in rows
        if layer == "package" and searched == "simple forward" and result.get("status") == "Optimal"
    }

    for instance, relaxed, layer, searched, result in rows:
        expected = reference.get((instance, relaxed))
        objective = result.get("objective")

        if expected is None or objective is None:
            continue

        if abs(objective - expected) > TOLERANCE * max(1.0, abs(expected)):
            failures.append(
                f"{instance} {relaxed} {layer} {searched}: LP {objective} != {expected}"
            )

    print("LP per instance and relaxation (the package's simple forward search):")

    for (instance, relaxed), objective in sorted(reference.items()):
        print(f"  {instance:9} {relaxed:11} {objective:.6f}")

    for instance in sorted({key[0] for key in reference}):
        none, ng8 = reference.get((instance, "none")), reference.get((instance, "ng 8"))

        if none is not None and ng8 is not None and none > ng8 + TOLERANCE * abs(ng8):
            failures.append(f"{instance}: none {none} > ng 8 {ng8}")

    ng10, elementary = reference.get(("C101_10", "ng 10")), reference.get(("C101_10", "elementary"))

    if ng10 is not None and elementary is not None and abs(ng10 - elementary) > TOLERANCE * ng10:
        failures.append(f"C101_10: ng 10 {ng10} != elementary {elementary}")

    if ("C101_10", "ng 8") in reference and ng10 is not None:
        if reference["C101_10", "ng 8"] > ng10 + TOLERANCE * ng10:
            failures.append("C101_10: ng 8 > ng 10")

    print(f"\n{len(rows)} runs; {len(failures)} failures")

    for failure in failures:
        print(f"  FAIL {failure}")

    times = defaultdict(list)

    for instance, relaxed, layer, searched, result in rows:
        if result.get("solve_seconds") is not None:
            times[instance, relaxed, layer, searched].append(result["solve_seconds"])

    print("\nsolve seconds:")

    for key in sorted(times):
        print(f"  {' | '.join(key):62} {statistics.median(times[key]):8.2f}")

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
