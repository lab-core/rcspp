"""Probes of the pure-Python layer (step G of plans/colgen/python-layer.md): what four open items wait on.

  python_layer_probes.py --instance R201_25 [--master highspy] [--repeats 5]

Runs the pure-Python loop (exact pricing, single-customer start) with timers around the calls the items are about,
then replays the columns the pricer returned through the layers' `add_columns`. Prints one JSON line `PROBE <json>`
and a readable table. The layer is not changed: everything is patched from here, in this process.

  B.15  copies of the HiGHS solution: `getSolution`, `getLp` calls and their time, against the master's time
  B.16  the batches' integer widths: the conversions `add_columns` makes, against the same with the widths HiGHS takes
  D.12  `MasterProblem.add_columns` against the C++-backed one, on the same solution lists; its parts
  copy  `ColumnBatch` keeping the caller's arrays or copying them
"""

import argparse
import json
import math
import os
import statistics
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)  # drivers.py
import drivers  # noqa: E402

clock = time.perf_counter


class Timer:
    """Adds up the calls and time of what it wraps."""

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


def best_of(function, repeats=7, inner=1):
    """The smallest time per call of ``function`` over ``repeats`` batches of ``inner`` calls, in seconds."""
    best = math.inf
    for _ in range(repeats):
        start = clock()
        for _ in range(inner):
            function()
        best = min(best, (clock() - start) / inner)
    return best


def build(name, master_kind="highspy", timed_model=None):
    """The pure-Python layer's master and pricer on an instance, single-customer start not yet added."""
    import rcspp.python_column_generation as pure
    from vrp.vrp import VRP

    class FiniteVRP(VRP):
        def _add_arc(self, graph, orig_id, dest_id, orig, dest):
            if not (orig.depot and dest.depot):
                super()._add_arc(graph, orig_id, dest_id, orig, dest)

    instance = drivers.read_instance(name)
    graph = FiniteVRP(instance).construct_resource_graph()
    rows = len(instance.get_customers_by_id())
    return pure, instance, graph, rows


def instrumented_run(name, master_kind):
    """One run of the pure-Python loop with timers; returns the timers, the batches and the solution lists."""
    import highspy
    import rcspp.python_column_generation as pure
    from rcspp.python_column_generation import master_problem as master_module
    from rcspp.python_column_generation.master_solvers import highspy_master_solver as solver_module

    pure, instance, graph, rows = build(name)
    timers = {key: Timer() for key in ("getSolution", "getLp", "getInfo", "addCols", "run", "row_bounds",
                                       "largest_user_cost", "duals", "primal", "objective")}

    if master_kind == "highspy":

        class TimedHighs(highspy.Highs):
            pass

        for method in ("getSolution", "getLp", "getInfo", "addCols", "run"):
            setattr(TimedHighs, method, timers[method].wrap(getattr(highspy.Highs, method)))
        model = TimedHighs()
        model.setOptionValue("output_flag", False)
        for _ in range(rows):
            lower = -math.inf if _ == 0 else 1.0
            model.addRow(lower, math.inf if _ == 0 else 1.0, 0, [], [])
        solver = pure.HighspyMasterSolver(model)
        for method in ("row_bounds", "largest_user_cost", "duals", "primal", "objective"):
            setattr(solver, method, timers[method].wrap(getattr(solver, method)))
    else:
        raise SystemExit("only highspy is probed: the copies of the solution are highspy's (B.15)")

    master = pure.MasterProblem(solver)
    pricer = master.add_pricer(pure.GraphPricer(graph, [pure.PricingStage("simple")]), pure.ConvexityRow())
    drivers.add_single_customer_routes(master, pricer, instance)

    # What add_columns is given and what it sends on.
    lists, batches = [], []
    timers["add_columns"], timers["solver_add_columns"] = Timer(), Timer()
    original_add = master_module.MasterProblem.add_columns
    original_solver_add = solver_module.HighspyMasterSolver.add_columns

    def spy_add(self, index, columns):
        columns = list(columns)
        lists.append(columns)
        return timers["add_columns"].wrap(original_add)(self, index, columns)

    def spy_solver_add(self, batch):
        batches.append(batch)
        return timers["solver_add_columns"].wrap(original_solver_add)(self, batch)

    master_module.MasterProblem.add_columns = spy_add
    solver_module.HighspyMasterSolver.add_columns = spy_solver_add
    try:
        start = clock()
        result = pure.ColumnGeneration(master).run()
        total = clock() - start
    finally:
        master_module.MasterProblem.add_columns = original_add
        solver_module.HighspyMasterSolver.add_columns = original_solver_add
    return dict(result=result, total=total, timers=timers, lists=lists, batches=batches, model=model, rows=rows,
                graph=graph, instance=instance)


def probe_solution_copies(run, repeats):
    """B.15: calls and time of the copies the master solver makes, and their cost at the final size."""
    timers, result, model = run["timers"], run["result"], run["model"]
    import highspy

    stats = result.statistics
    final_columns = model.getNumCol()
    lp = highspy.Highs.getLp(model)
    solution_time = best_of(lambda: highspy.Highs.getSolution(model), repeats=repeats, inner=20)
    lp_time = best_of(lambda: highspy.Highs.getLp(model), repeats=repeats, inner=20)
    info_time = best_of(lambda: highspy.Highs.getInfo(model), repeats=repeats, inner=200)
    return dict(
        iterations=result.iterations,
        master_s=stats.relaxation_seconds,
        pricing_s=stats.pricing_seconds,
        run_s=run["total"],
        getSolution_calls=timers["getSolution"].calls,
        getSolution_s=timers["getSolution"].seconds,
        getLp_calls=timers["getLp"].calls,
        getLp_s=timers["getLp"].seconds,
        row_bounds_calls=timers["row_bounds"].calls,
        largest_user_cost_calls=timers["largest_user_cost"].calls,
        duals_calls=timers["duals"].calls,
        duals_s=timers["duals"].seconds,
        primal_calls=timers["primal"].calls,
        final_columns=final_columns,
        final_entries=lp.a_matrix_.index_.__len__() if hasattr(lp.a_matrix_, "index_") else None,
        getSolution_final_us=solution_time * 1e6,
        getLp_final_us=lp_time * 1e6,
        getInfo_us=info_time * 1e6,
    )


def probe_widths(run, repeats):
    """B.16: the integer conversions of the batches `add_columns` sends, and the same without them."""
    batches = run["batches"]
    rows = np.arange(run["rows"], dtype=np.int32)  # the master solver's _rows

    def conversions_now():
        for batch in batches:
            # what MasterProblem._send does before the batch exists, and what the master solver does with it
            batch.starts[:-1].astype(np.int32)
            rows[batch.row_indices]

    def conversions_int32():
        for batch in batches:
            batch.starts[:-1]  # no cast: starts already int32
            rows[batch.row_indices.view(np.int32) if batch.row_indices.dtype == np.uint32 else batch.row_indices]

    # The conversions the master problem makes while building a batch, from int64 arrays to uint32.
    sizes = [len(b.row_indices) for b in batches]
    int64_rows = [b.row_indices.astype(np.int64) for b in batches]
    int64_lengths = [np.diff(b.starts.astype(np.int64)) for b in batches]

    def build_uint32():
        for rows64, lengths in zip(int64_rows, int64_lengths):
            starts = np.zeros(len(lengths) + 1, dtype=np.uint32)
            starts[1:] = np.cumsum(lengths)
            rows64.astype(np.uint32)

    def build_int32():
        for rows64, lengths in zip(int64_rows, int64_lengths):
            starts = np.zeros(len(lengths) + 1, dtype=np.int32)
            starts[1:] = np.cumsum(lengths)
            rows64.astype(np.int32)

    def column_batch(dtype_starts, dtype_rows):
        for b in batches:
            drivers_ids = np.asarray(b.ids)
            args = (drivers_ids, b.cost, b.lower, b.upper, b.starts.astype(dtype_starts),
                    b.row_indices.astype(dtype_rows), b.values)
            run["ColumnBatch"](*args)

    return dict(
        batches=len(batches),
        mean_columns=statistics.mean(len(b) for b in batches),
        mean_entries=statistics.mean(sizes),
        solver_side_now_us=best_of(conversions_now, repeats) * 1e6,
        solver_side_int32_us=best_of(conversions_int32, repeats) * 1e6,
        build_uint32_us=best_of(build_uint32, repeats) * 1e6,
        build_int32_us=best_of(build_int32, repeats) * 1e6,
        column_batch_uint32_us=best_of(lambda: column_batch(np.uint32, np.uint32), repeats) * 1e6,
        column_batch_int32_us=best_of(lambda: column_batch(np.int32, np.int32), repeats) * 1e6,
    )


def fresh_master(layer, name, master_kind="highspy"):
    """A master of ``layer`` (pure or C++-backed) on the instance, with the single-customer start."""
    from vrp.vrp import VRP

    class FiniteVRP(VRP):
        def _add_arc(self, graph, orig_id, dest_id, orig, dest):
            if not (orig.depot and dest.depot):
                super()._add_arc(graph, orig_id, dest_id, orig, dest)

    instance = drivers.read_instance(name)
    graph = FiniteVRP(instance).construct_resource_graph()
    rows = len(instance.get_customers_by_id())
    master = layer.MasterProblem(drivers.master_model(layer, master_kind, rows))
    pricer = master.add_pricer(layer.GraphPricer(graph, [layer.PricingStage("simple")]), layer.ConvexityRow())
    drivers.add_single_customer_routes(master, pricer, instance)
    return master


def probe_add_columns(run, name, repeats):
    """D.12: `add_columns` of each layer over the solution lists the run produced, and the pure one's parts."""
    import rcspp.column_generation as cpp
    import rcspp.python_column_generation as pure

    # What the loop's pricing returned, without the empty last call (the spy was put in after the single-customer
    # start, which fresh_master adds itself).
    lists = [solutions for solutions in run["lists"] if solutions]

    def replay(layer):
        master = fresh_master(layer, name)
        seconds = []
        for solutions in lists:
            start = clock()
            master.add_columns(0, solutions)
            seconds.append(clock() - start)
        return seconds

    results = {}
    for label, layer in (("pure", pure), ("cpp", cpp)):
        runs = [replay(layer) for _ in range(repeats)]
        per_call = [min(column) for column in zip(*runs)]
        results[label] = dict(total_ms=sum(per_call) * 1e3, per_call_ms=[round(x * 1e3, 3) for x in per_call])

    # The pure layer's parts, on the same lists.
    to_arrays = best_of(lambda: [[s.column.to_arrays() for s in solutions] for solutions in lists], repeats)
    columns = sum(len(solutions) for solutions in lists)

    def concatenate_all():
        for solutions in lists:
            parts = [s.column.to_arrays() for s in solutions]
            np.concatenate([p[1] for p in parts])
            np.concatenate([p[2] for p in parts])

    to_arrays_and_concatenate = best_of(concatenate_all, repeats)
    solver_calls = run["timers"]["solver_add_columns"]
    results["parts"] = dict(
        columns=columns,
        to_arrays_ms=to_arrays * 1e3,
        to_arrays_per_column_us=to_arrays / columns * 1e6,
        to_arrays_and_concatenate_ms=to_arrays_and_concatenate * 1e3,
        solver_add_columns_ms_in_run=solver_calls.seconds * 1e3,
        add_columns_ms_in_run=run["timers"]["add_columns"].seconds * 1e3,
        add_columns_calls_in_run=run["timers"]["add_columns"].calls,
    )
    return results


def probe_copy(run, repeats):
    """Whether `ColumnBatch` should copy its inputs: its constructor sharing the arrays, and copying them."""
    from rcspp.python_column_generation import ColumnBatch

    batches = run["batches"]
    arrays = [
        tuple(np.array(getattr(b, field)) for field in ("ids", "cost", "lower", "upper", "starts", "row_indices",
                                                        "values"))
        for b in batches
    ]

    def share():
        for args in arrays:
            ColumnBatch(*args)

    def copy():
        for args in arrays:
            ColumnBatch(*[np.array(a) for a in args])

    def copy_only():
        for args in arrays:
            [np.array(a) for a in args]

    # Whether a batch follows its caller's array today.
    batch = ColumnBatch(*arrays[0])
    before = batch.cost[0]
    arrays[0][1][0] += 1.0
    follows_caller = bool(batch.cost[0] != before)
    arrays[0][1][0] -= 1.0
    return dict(
        batches=len(batches),
        share_us=best_of(share, repeats) * 1e6,
        copy_us=best_of(copy, repeats) * 1e6,
        copy_only_us=best_of(copy_only, repeats) * 1e6,
        batch_follows_callers_array=follows_caller,
        batch_arrays_writable=any(getattr(batch, f).flags.writeable for f in ("ids", "cost", "values")),
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--instance", required=True)
    parser.add_argument("--master", default="highspy", choices=["highspy"])
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()

    from rcspp.python_column_generation import ColumnBatch

    run = instrumented_run(args.instance, args.master)
    run["ColumnBatch"] = ColumnBatch
    report = dict(
        instance=args.instance,
        lp=run["result"].objective,
        status=run["result"].status.name,
        B15=probe_solution_copies(run, args.repeats),
        B16=probe_widths(run, args.repeats),
        D12=probe_add_columns(run, args.instance, args.repeats),
        copy=probe_copy(run, args.repeats),
    )
    print("PROBE " + json.dumps(report, default=float), flush=True)
    for key in ("B15", "B16", "copy"):
        print(f"\n{key}")
        for name, value in report[key].items():
            print(f"  {name:32s} {value:.4f}" if isinstance(value, float) else f"  {name:32s} {value}")
    print("\nD12")
    for label in ("pure", "cpp"):
        print(f"  {label}: total {report['D12'][label]['total_ms']:.3f} ms over {len(report['D12'][label]['per_call_ms'])} calls")
    for name, value in report["D12"]["parts"].items():
        print(f"  {name:32s} {value:.4f}" if isinstance(value, float) else f"  {name:32s} {value}")


if __name__ == "__main__":
    main()
