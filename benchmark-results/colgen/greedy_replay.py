"""Replays the duals of an exact run through greedy and simple pricing (plans: T-1.14 step 1, F-40).

Runs the Python example on an instance, records the duals of every iteration, then prices the first, middle and
last duals with `simple` (exact) and `greedy` (capped, with a timeout), with the layer's threshold.

  python greedy_replay.py --instance R201_50 [--cap 100] [--timeout 60]
"""

import argparse
import contextlib
import io
import math
import os
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "examples", "python"))

from rcspp.graph import AlgorithmParams  # noqa: E402
from vrp.instance_reader import InstanceReader  # noqa: E402
from vrp.vrp import VRP  # noqa: E402

THRESHOLD = -1e-6  # a path counts only below it, as in the layer's graph pricer


def record_duals(name):
    """Runs the example to convergence and returns its graph and the duals of each iteration."""
    instance = InstanceReader(os.path.join(ROOT, "instances", name + ".txt")).read()
    vrp = VRP(instance)
    duals = []
    solve_subproblem = vrp.solve_subproblem

    def recording(dual_by_id=None):
        duals.append(dict(dual_by_id))
        return solve_subproblem(dual_by_id)

    vrp.solve_subproblem = recording
    start = time.perf_counter()
    with contextlib.redirect_stdout(io.StringIO()):  # the example prints every iteration
        vrp.solve()
    print(f"example: {len(duals)} iterations in {time.perf_counter() - start:.1f} s")
    return vrp._VRP__resource_graph, duals


def price(graph, algorithm, params):
    start = time.perf_counter()
    result = graph.solve(algorithm, upper_bound=THRESHOLD, params=params)
    seconds = time.perf_counter() - start
    best = min((s.cost for s in result.solutions), default=math.inf)
    status = str(result.status).split(".")[-1]
    return f"{seconds:8.2f} s  {status:<12} {len(result.solutions):>6} paths  best {best:.4g}"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--instance", default="R201_50")
    parser.add_argument("--cap", type=int, default=100)
    parser.add_argument("--timeout", type=float, default=60.0)
    args = parser.parse_args()

    graph, duals = record_duals(args.instance)
    picks = {"first": 0, "middle": len(duals) // 2, "last": len(duals) - 1}

    for label, index in picks.items():
        graph.update_reduced_costs(duals[index])
        greedy = AlgorithmParams()
        greedy.stop_after_X_solutions = args.cap
        greedy.timeout_s = args.timeout
        print(f"\n{label} duals (iteration {index + 1} of {len(duals)})")
        print(f"  simple  {price(graph, 'simple', AlgorithmParams())}")
        print(f"  greedy  {price(graph, 'greedy', greedy)}")


if __name__ == "__main__":
    main()
