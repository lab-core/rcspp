"""MEASUREMENT ONLY: old-vrp and new-vrp alternated, rounds x instances, into a JSON-lines file;
then a markdown table of medians.

    python run.py --instances C101_25,R101_25,R201_25,RC201_12 --rounds 5 --output small.jsonl
    python run.py --summarize small.jsonl
"""

import argparse
import json
import statistics
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROGRAMS = {side: HERE / "build" / "Release" / f"{side}-vrp.exe" for side in ("old", "new", "replay")}
INSTANCES = HERE.parents[3] / "rcspp-dev" / "instances"


def run(output: Path, instances: list[str], rounds: int, replay: bool = False) -> None:
    with output.open("a", encoding="utf-8") as file:
        for round_number in range(1, rounds + 1):
            for instance in instances:
                # Alternate which side goes first, round by round.
                sides = ("old", "new") if round_number % 2 else ("new", "old")
                if replay:
                    sides = ("replay",)
                for side in sides:
                    completed = subprocess.run(
                        [str(PROGRAMS[side]), str(INSTANCES / f"{instance}.txt")],
                        capture_output=True,
                        text=True,
                        cwd=PROGRAMS[side].parent,
                    )
                    line = next(
                        (l for l in completed.stdout.splitlines() if l.startswith("RESULT ")), None
                    )
                    if line is None:
                        sys.exit(f"{side} {instance}: no RESULT\n{completed.stdout}{completed.stderr}")
                    record = json.loads(line[len("RESULT ") :])
                    record.update(instance=instance, round=round_number)
                    file.write(json.dumps(record) + "\n")
                    file.flush()
                    print(f"r{round_number} {instance} {side}: {line}")


def solve_seconds(record: dict) -> float:
    """The column generation from the first master solve to the end, setup left out."""
    return sum(record.get(part, 0.0) for part in ("master_s", "duals_s", "search_s", "add_s", "rest_s"))


def summarize(path: Path) -> None:
    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]
    instances = list(dict.fromkeys(record["instance"] for record in records))

    def med(values):
        return statistics.median(values)

    print("| instance | LP old | LP new | rel. diff | it. old / new | col. old / new | solve s old | "
          "solve s new [min-max] | new - old | rule |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    parts_rows = []
    for instance in instances:
        sides = {s: [r for r in records if r["instance"] == instance and r["side"] == s] for s in ("old", "new")}
        old, new = sides["old"], sides["new"]
        lp_old, lp_new = old[0]["lp"], new[0]["lp"]
        rel = abs(lp_new - lp_old) / abs(lp_old)
        it = lambda rs: "/".join(sorted({str(r["iterations"]) for r in rs}))
        col = lambda rs: "/".join(sorted({str(r["columns"]) for r in rs}))
        t_old = med([solve_seconds(r) for r in old])
        new_times = [solve_seconds(r) for r in new]
        t_new = med(new_times)
        limit = max(0.05 * t_old, 0.25)
        verdict = "pass" if rel <= 1e-6 and t_new - t_old <= limit else "FAIL"
        print(f"| {instance} | {lp_old:.6f} | {lp_new:.6f} | {rel:.1e} | {it(old)} / {it(new)} | "
              f"{col(old)} / {col(new)} | {t_old:.4f} | {t_new:.4f} [{min(new_times):.4f}-{max(new_times):.4f}] | "
              f"{t_new - t_old:+.4f} | {verdict} |")
        for side, rs in (("old", old), ("new", new)):
            iterations = med([r["iterations"] for r in rs])
            row = {part: med([r.get(part, 0.0) for r in rs]) for part in
                   ("master_s", "duals_s", "search_s", "add_s", "rest_s")}
            parts_rows.append((instance, side, iterations, row))

    print()
    print("Per iteration, ms (medians over the rounds of each part, divided by the median iterations); "
          "outside = search + add + duals + rest:")
    print()
    print("| instance | side | iterations | master | search | add | duals + rest | outside the master |")
    print("|---|---|---|---|---|---|---|---|")
    for instance, side, iterations, row in parts_rows:
        per = {k: 1000 * v / iterations for k, v in row.items()}
        outside = per["search_s"] + per["add_s"] + per["duals_s"] + per["rest_s"]
        print(f"| {instance} | {side} | {iterations:g} | {per['master_s']:.3f} | {per['search_s']:.3f} | "
              f"{per['add_s']:.3f} | {per['duals_s'] + per['rest_s']:.3f} | {outside:.3f} |")


def summarize_replay(path: Path) -> None:
    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]
    instances = list(dict.fromkeys(record["instance"] for record in records))
    med = statistics.median
    print("| instance | iterations | columns (both) | differing iterations | pricing ms/it old | layer "
          "[min-max] | layer / old | add us/column old | layer | layer - old, ms/it |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for instance in instances:
        rs = [r for r in records if r["instance"] == instance]
        it = rs[0]["iterations"]
        cols = rs[0]["columns"]
        assert all(r["columns"] == r["layer_columns"] for r in rs)
        differ = max(r["count_differs"] + r["best_differs"] for r in rs)
        old_p = med([r["old_search_s"] for r in rs]) * 1000 / it
        new_ps = [r["layer_price_s"] * 1000 / it for r in rs]
        new_p = med(new_ps)
        old_a = med([r["old_add_s"] for r in rs]) * 1e6 / cols
        new_a = med([r["layer_add_s"] for r in rs]) * 1e6 / cols
        delta = (new_p - old_p) + (new_a - old_a) * cols / it / 1000
        print(f"| {instance} | {it} | {cols} | {differ} | {old_p:.3f} | {new_p:.3f} "
              f"[{min(new_ps):.3f}-{max(new_ps):.3f}] | {new_p / old_p:.3f} | {old_a:.2f} | {new_a:.2f} | "
              f"{delta:+.3f} |")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--instances")
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("--output")
    parser.add_argument("--summarize")
    parser.add_argument("--replay", action="store_true")
    parser.add_argument("--summarize-replay")
    arguments = parser.parse_args()
    if arguments.summarize:
        summarize(Path(arguments.summarize))
    elif arguments.summarize_replay:
        summarize_replay(Path(arguments.summarize_replay))
    else:
        run(Path(arguments.output), arguments.instances.split(","), arguments.rounds, arguments.replay)


if __name__ == "__main__":
    main()
