"""One run of the Python loop with fixes A and B switched on or off; the last line printed is `RESULT <json>`.

  ab_run.py INSTANCE [--master highs|fallback] [--factor 1e4] [--pricing exact|tabu] [--init artificial|single]

--master highs: the library's HighsMasterSolver (no retry); fallback: the prototype that solves again from scratch.
--factor: the artificial cost is factor times the cost scale (the library's automatic factor is 1e6).
--pricing tabu: tabu (cap 20, max_iterations 1000) then simple, as `D-117` suggests; exact: simple alone.
"""

import argparse
import json
import os
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "benchmark-results", "colgen"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drivers  # noqa: E402  # puts the examples on the path

import column_generation_loop  # noqa: E402
import rcspp.column_generation as cg  # noqa: E402
from fallback_master import FallbackHighspyMasterSolver, SmallerArtificialColumns  # noqa: E402
from rcspp.graph import AlgorithmParams  # noqa: E402
from vrp.vrp import VRP  # noqa: E402


class FiniteVRP(VRP):
    def _add_arc(self, graph, orig_id, dest_id, orig, dest):
        if not (orig.depot and dest.depot):  # the example's infinite empty route: not a column
            super()._add_arc(graph, orig_id, dest_id, orig, dest)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("instance")
    parser.add_argument("--master", default="fallback", choices=["highs", "fallback"])
    parser.add_argument("--factor", type=float, default=1e4)
    parser.add_argument("--pricing", default="exact", choices=["exact", "tabu"])
    parser.add_argument("--init", default="artificial", choices=["artificial", "single"])
    args = parser.parse_args()

    instance = drivers.read_instance(args.instance)
    graph = FiniteVRP(instance).construct_resource_graph()
    rows = len(instance.get_customers_by_id())  # row 0 is free: the depot has no row
    lower = [-float("inf")] + [1.0] * (rows - 1)
    upper = [float("inf")] + [1.0] * (rows - 1)
    if args.master == "fallback":
        solver = FallbackHighspyMasterSolver(lower, upper, progress=os.environ.get("AB_PROGRESS"))
    else:
        solver = cg.HighsMasterSolver(row_lower=lower, row_upper=upper)

    stages = [cg.PricingStage("simple")]
    if args.pricing == "tabu":
        tabu = AlgorithmParams()
        tabu.stop_after_X_solutions = 20
        tabu.max_iterations = 1000
        stages = [cg.PricingStage("tabu", tabu), cg.PricingStage("simple")]

    master = cg.MasterProblem(solver)
    pricer = master.add_pricer(cg.GraphPricer(graph, stages), cg.ConvexityRow())
    params = cg.ColumnGenerationParams()
    if args.init == "single":
        drivers.add_single_customer_routes(master, pricer, instance)
    else:
        params.initialization = SmallerArtificialColumns(args.factor)

    start = time.perf_counter()
    result = column_generation_loop.run(master, params)
    solve = time.perf_counter() - start
    drivers.emit(
        instance=args.instance,
        master=args.master,
        factor=args.factor if args.init == "artificial" else None,
        pricing=args.pricing,
        init=args.init,
        status=str(result.status).split(".")[-1],
        lp=result.objective,
        iterations=result.iterations,
        columns=result.generated_columns,
        artificial_cost=result.artificial_cost,
        solve_s=solve,
        master_s=result.relaxation_seconds,
        pricing_s=result.pricing_seconds,
        retries=getattr(solver, "retries", None),
    )


if __name__ == "__main__":
    main()
