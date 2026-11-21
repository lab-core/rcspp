"""The column generation alone, Python example against the pure layer: loop runs (drivers.py, both sides alternated
per round) and replays (replay.py), each run a subprocess; then the tables.

  python run.py loop --instances C101_25,R201_25 --rounds 5 --output loop.jsonl
  python run.py replay --instances C101_25,R201_25 --rounds 3 --output replay.jsonl
  python run.py summarize loop.jsonl replay.jsonl

The source checkout is COLGEN_SOURCE_DIRECTORY (default ../rcspp-benchmark next to this repository: #97's code, with
the old Python example and a built _core in build-python-highs). Outputs append.
"""

import argparse
import json
import os
import statistics
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPOSITORY = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SOURCE = os.path.abspath(
    os.environ.get("COLGEN_SOURCE_DIRECTORY") or os.path.join(REPOSITORY, "..", "rcspp-benchmark")
)
SIDES = {
    "example": ["example"],
    "layer": ["layer", "--loop", "purepython", "--master", "gurobipy"],
}


def environment():
    variables = dict(os.environ)
    variables["COLGEN_SOURCE_DIRECTORY"] = SOURCE
    variables["COLGEN_INIT"] = "single"
    variables["PYTHONPATH"] = os.pathsep.join(
        [
            os.path.join(SOURCE, "build-python-highs", "python"),
            os.path.join(SOURCE, "python", "src"),
            os.path.join(SOURCE, "examples", "column_generation", "python"),
        ]
    )
    return variables


def result_of(command):
    completed = subprocess.run(command, capture_output=True, text=True, env=environment())
    line = next((l for l in reversed(completed.stdout.splitlines()) if l.startswith("RESULT ")), None)
    if line is None:
        sys.exit(f"no RESULT from {command}\n{completed.stdout[-2000:]}{completed.stderr[-2000:]}")
    return json.loads(line[len("RESULT ") :])


def run(mode, instances, rounds, output):
    with open(output, "a", encoding="utf-8") as file:
        for round_number in range(1, rounds + 1):
            for instance in instances:
                if mode == "loop":
                    order = ("example", "layer") if round_number % 2 else ("layer", "example")
                    commands = [
                        (side, [sys.executable, os.path.join(HERE, "..", "drivers.py"), *SIDES[side],
                                "--instance", instance])
                        for side in order
                    ]
                else:
                    commands = [("replay", [sys.executable, os.path.join(HERE, "replay.py"), "--instance", instance])]
                for side, command in commands:
                    record = result_of(command)
                    record.update(instance=instance, side=side, round=round_number)
                    file.write(json.dumps(record) + "\n")
                    file.flush()
                    print(f"r{round_number} {instance} {side}: iterations {record['iterations']}", flush=True)


def median(values):
    return statistics.median(values) if values else float("nan")


def own(record):
    """The column generation's own seconds in a loop run: the LP part left of the master, search, reduced costs and
    the LP solver's column adds."""
    lp = record["master_s"] + record["pricing_s"] + record["rest_s"]
    engine = record["search_s"] + record["reduced_costs_s"] + record["solver_add_s"]
    return lp - record["master_s"] - engine


def summarize_loop(records):
    instances = list(dict.fromkeys(r["instance"] for r in records))
    print("Loop runs: medians over the rounds. Own = the LP part outside the master's solve, the search, the arcs'")
    print("reduced costs and the LP solver's column adds. Per iteration and per column use the median iterations.\n")
    print("| instance | side | it. | col. | LP s | master | search | reduced costs | solver adds | own, ms/it. "
          "| own, us/col. | own s | own % of LP |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for instance in instances:
        for side in ("example", "layer"):
            rs = [r for r in records if r["instance"] == instance and r["side"] == side]
            if not rs:
                continue
            it = median([r["iterations"] for r in rs])
            col = median([r["columns"] for r in rs])
            lp = median([r["master_s"] + r["pricing_s"] + r["rest_s"] for r in rs])
            mine = median([own(r) for r in rs])

            def per_it(key):
                return 1e3 * median([r[key] for r in rs]) / it

            spread = sorted({r["iterations"] for r in rs})
            its = f"{spread[0]}-{spread[-1]}" if len(spread) > 1 else f"{spread[0]}"
            print(f"| {instance} | {side} | {its} | {col:.0f} | {lp:.3f} | {per_it('master_s'):.3f} "
                  f"| {per_it('search_s'):.3f} | {per_it('reduced_costs_s'):.3f} | {per_it('solver_add_s'):.3f} "
                  f"| {1e3 * mine / it:.3f} | {1e6 * mine / col:.1f} | {mine:.3f} | {100 * mine / lp:.1f} |")


def summarize_replay(records):
    instances = list(dict.fromkeys(r["instance"] for r in records))
    print("\nReplay: the example's loop; at each iteration the layer's pricing step on the same duals. Medians over")
    print("the rounds, ms per iteration; own = the step outside search, reduced costs and the LP solver's adds.\n")
    print("| instance | it. | col. | differing it. | search old / layer | reduced costs old / layer "
          "| solver adds old / layer | own old / layer | own us/col. old / layer | layer - old own, s per run |")
    print("|---|---|---|---|---|---|---|---|---|---|")
    for instance in instances:
        rs = [r for r in records if r["instance"] == instance]
        # The example may take another path in each round (C201): every value is per round, then the median.
        its = sorted({r["iterations"] for r in rs})
        cols = sorted({r["columns"] for r in rs})

        def own_of(record, side):
            part = record[side]
            return part["total"] - part["search_s"] - part["reduced_costs_s"] - part["solver_add_s"]

        def per_iteration(side, key):
            return median([1e3 * r[side][key] / r["iterations"] for r in rs])

        def pair(key):
            return f"{per_iteration('old', key):.3f} / {per_iteration('layer', key):.3f}"

        def own_per(side, scale, count):
            return median([scale * own_of(r, side) / r[count] for r in rs])

        gap = median([own_of(r, "layer") - own_of(r, "old") for r in rs])
        it = f"{its[0]}-{its[-1]}" if len(its) > 1 else f"{its[0]}"
        col = f"{cols[0]}-{cols[-1]}" if len(cols) > 1 else f"{cols[0]}"
        print(f"| {instance} | {it} | {col} | {max(r['differing_iterations'] for r in rs)} | {pair('search_s')} "
              f"| {pair('reduced_costs_s')} | {pair('solver_add_s')} "
              f"| {own_per('old', 1e3, 'iterations'):.3f} / {own_per('layer', 1e3, 'iterations'):.3f} "
              f"| {own_per('old', 1e6, 'columns'):.1f} / {own_per('layer', 1e6, 'columns'):.1f} "
              f"| {gap:+.3f} |")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["loop", "replay", "summarize"])
    parser.add_argument("files", nargs="*")
    parser.add_argument("--instances")
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.mode == "summarize":
        records = [json.loads(l) for f in args.files for l in open(f, encoding="utf-8")]
        loop = [r for r in records if r["side"] != "replay"]
        replay = [r for r in records if r["side"] == "replay"]
        if loop:
            summarize_loop(loop)
        if replay:
            summarize_replay(replay)
        return
    run(args.mode, args.instances.split(","), args.rounds, args.output)


if __name__ == "__main__":
    main()
