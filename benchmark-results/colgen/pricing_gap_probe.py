"""Where the pure-Python layer's extra time goes (step H of plans/colgen/python-layer.md): its pricing slice, split.

  pricing_gap_probe.py --instance R201_25 [--pricing exact|heuristic|tabu] [--cap 100] [--repeats 7]

Alternates C++-backed and pure-Python runs in one process (highspy master, single-customer start, the driver's
graph and stages), `--repeats` of each. In the pure runs, timers split the pricing slice (which holds
`add_columns` in both layers):

  urc_inner / urc_wrapper     the C++ update_reduced_costs call / ResourceGraph.update_reduced_costs around it
  solve_inner / solve_wrapper the C++ solve call / ResourceGraph.solve around it (flush, params, direction)
  to_result                   _to_pricing_result: the SolveResult.solutions list built in Python, the filter
  price                       GraphPricer.price (all of the above, the stage loop, StageRequest)
  add_columns                 MasterProblem.add_columns; to_arrays: the Column.to_arrays calls inside it
  loop_price                  the pricing slice minus price and add_columns (the loop's _price)

Replays (best of 5): `solutions_list`, building SolveResult.solutions again for every call of the run.
Prints `PROBE <json>` and a table of medians, in ms per run. The layer is not changed.
"""

import argparse
import json
import os
import statistics
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import drivers  # noqa: E402

clock = time.perf_counter


class Timer:
    def __init__(self):
        self.calls, self.seconds = 0, 0.0

    def wrap(self, function):
        def timed(*args, **kwargs):
            start = clock()
            try:
                return function(*args, **kwargs)
            finally:
                self.calls += 1
                self.seconds += clock() - start

        return timed


class TimedGraph:
    """Forwards to the C++ graph, timing solve and update_reduced_costs; keeps the SolveResults."""

    def __init__(self, inner, timers, results):
        self._inner, self._timers, self._results = inner, timers, results

    def solve(self, *args):
        result = self._timers["solve_inner"].wrap(self._inner.solve)(*args)
        self._results.append(result)
        return result

    def update_reduced_costs(self, *args):
        return self._timers["urc_inner"].wrap(self._inner.update_reduced_costs)(*args)

    def __getattr__(self, name):
        return getattr(self._inner, name)


def setup(loop, name, pricing, cap):
    cg = drivers.layer_module(loop)
    from rcspp.graph import AlgorithmParams
    from vrp.vrp import VRP

    class FiniteVRP(VRP):
        def _add_arc(self, graph, orig_id, dest_id, orig, dest):
            if not (orig.depot and dest.depot):
                super()._add_arc(graph, orig_id, dest_id, orig, dest)

    instance = drivers.read_instance(name)
    graph = FiniteVRP(instance).construct_resource_graph()
    rows = len(instance.get_customers_by_id())
    stages = [cg.PricingStage("simple")]
    if pricing == "heuristic":  # the driver's heuristic configuration: capped greedy, then simple
        greedy = AlgorithmParams()
        greedy.stop_after_X_solutions = cap
        stages = [cg.PricingStage("greedy", greedy), cg.PricingStage("simple")]
    elif pricing == "tabu":  # tabu then simple, as the big-M runs (cap 20, 1000 iterations)
        tabu = AlgorithmParams()
        tabu.stop_after_X_solutions = cap
        tabu.max_iterations = 1000
        stages = [cg.PricingStage("tabu", tabu), cg.PricingStage("simple")]
    master = cg.MasterProblem(drivers.master_model(cg, "highspy", rows))
    pricer = master.add_pricer(cg.GraphPricer(graph, stages), cg.ConvexityRow())
    drivers.add_single_customer_routes(master, pricer, instance)
    return cg, master, graph


def run_cpp(name, pricing, cap):
    cg, master, _ = setup("cpp", name, pricing, cap)
    start = clock()
    result = cg.ColumnGeneration(master).run()
    total = clock() - start
    stats = result.statistics
    return dict(lp=result.objective, iterations=result.iterations, columns=stats.generated_columns,
                total=total, master=stats.relaxation_seconds, pricing=stats.pricing_seconds,
                rest=total - stats.relaxation_seconds - stats.pricing_seconds)


def run_pure(name, pricing, cap):
    import rcspp._core.graph as core_graph
    from rcspp import graph as graph_module
    from rcspp.python_column_generation import graph_pricer as pricer_module
    from rcspp.python_column_generation import master_problem as master_module

    cg, master, graph = setup("purepython", name, pricing, cap)
    keys = ("urc_inner", "urc_wrapper", "solve_inner", "solve_wrapper", "to_result", "price", "add_columns",
            "to_arrays")
    timers = {key: Timer() for key in keys}
    results = []
    graph._graph = TimedGraph(graph._graph, timers, results)
    saved = dict(
        urc=graph_module.ResourceGraph.update_reduced_costs,
        solve=graph_module.ResourceGraph.solve,
        to_result=pricer_module._to_pricing_result,
        price=pricer_module.GraphPricer.price,
        add=master_module.MasterProblem.add_columns,
        to_arrays=core_graph.Column.to_arrays,
    )
    graph_module.ResourceGraph.update_reduced_costs = timers["urc_wrapper"].wrap(saved["urc"])
    graph_module.ResourceGraph.solve = timers["solve_wrapper"].wrap(saved["solve"])
    pricer_module._to_pricing_result = timers["to_result"].wrap(saved["to_result"])
    pricer_module.GraphPricer.price = timers["price"].wrap(saved["price"])
    master_module.MasterProblem.add_columns = timers["add_columns"].wrap(saved["add"])
    core_graph.Column.to_arrays = timers["to_arrays"].wrap(saved["to_arrays"])
    try:
        start = clock()
        result = cg.ColumnGeneration(master).run()
        total = clock() - start
    finally:
        graph_module.ResourceGraph.update_reduced_costs = saved["urc"]
        graph_module.ResourceGraph.solve = saved["solve"]
        pricer_module._to_pricing_result = saved["to_result"]
        pricer_module.GraphPricer.price = saved["price"]
        master_module.MasterProblem.add_columns = saved["add"]
        core_graph.Column.to_arrays = saved["to_arrays"]
        graph._graph = graph._graph._inner
    stats = result.statistics
    row = dict(lp=result.objective, iterations=result.iterations, columns=stats.generated_columns,
               total=total, master=stats.relaxation_seconds, pricing=stats.pricing_seconds,
               rest=total - stats.relaxation_seconds - stats.pricing_seconds,
               solve_calls=timers["solve_inner"].calls, paths=sum(len(r.solutions) for r in results))
    row.update({key: timer.seconds for key, timer in timers.items()})
    # Timers nest: wrapper times hold the inner ones, price holds both wrappers and to_result.
    row["add_columns_without_to_arrays"] = row["add_columns"] - row["to_arrays"]
    row["loop_price"] = row["pricing"] - row["price"] - row["add_columns"]
    row["price_python"] = row["price"] - row["urc_inner"] - row["solve_inner"]
    best = float("inf")
    for _ in range(5):
        start = clock()
        for r in results:
            r.solutions
        best = min(best, clock() - start)
    row["solutions_list"] = best
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--instance", required=True)
    parser.add_argument("--pricing", default="exact", choices=["exact", "heuristic", "tabu"])
    parser.add_argument("--cap", type=int, default=100)
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()

    rows = {"cpp": [], "pure": []}
    for _ in range(args.repeats):
        rows["cpp"].append(run_cpp(args.instance, args.pricing, args.cap))
        rows["pure"].append(run_pure(args.instance, args.pricing, args.cap))
    lps = {(loop, r["lp"], r["iterations"], r["columns"]) for loop in rows for r in rows[loop]}
    medians = {loop: {k: statistics.median(r[k] for r in rows[loop]) for k in rows[loop][0]} for loop in rows}
    report = dict(instance=args.instance, pricing=args.pricing, cap=args.cap, repeats=args.repeats,
                  outcomes=sorted(lps), medians=medians, runs=rows)
    print("PROBE " + json.dumps(report), flush=True)
    print(f"{args.instance} {args.pricing} (cap {args.cap}), {args.repeats} repeats, medians in ms")
    for outcome in sorted(lps):
        print("  outcome", outcome)
    for loop in rows:
        print(f"  {loop}: " + ", ".join(
            f"{k} {v:.0f}" if k in ("iterations", "columns", "solve_calls", "paths")
            else f"{k} {v}" if k == "lp" else f"{k} {v * 1e3:.2f}"
            for k, v in medians[loop].items()))


if __name__ == "__main__":
    main()
