"""One column generation run in this process; the last line printed is `RESULT <json>`.

  drivers.py example --instance C101_25
  drivers.py layer --instance C101_25 --loop cpp|python --master highs|highspy|gurobipy --pricing exact|heuristic
"""

import argparse
import json
import math
import os
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "examples", "python"))
sys.path.insert(0, os.path.join(ROOT, "instances"))


def read_instance(name):
    from vrp.instance_reader import InstanceReader

    return InstanceReader(os.path.join(ROOT, "instances", name + ".txt")).read()


def emit(**fields):
    print("RESULT " + json.dumps(fields), flush=True)


def run_example(name):
    """The current Python example (VRP.solve, python-mip master), timed from outside its code."""
    import vrp.cg.master_problem as master_module
    from vrp.vrp import VRP

    instance = read_instance(name)
    lp = {"value": math.nan, "iterations": 0, "master": 0.0, "integer": 0.0}
    original = master_module.MasterProblem.solve

    def timed_solve(self, relax=False):
        start = time.perf_counter()
        solution = original(self, relax)
        elapsed = time.perf_counter() - start
        if relax:
            lp["value"] = solution.cost
            lp["iterations"] += 1
            lp["master"] += elapsed
        else:
            lp["integer"] += elapsed
        return solution

    master_module.MasterProblem.solve = timed_solve
    vrp = VRP(instance)
    start = time.perf_counter()
    vrp.solve()
    solve = time.perf_counter() - start
    pricing = vrp._VRP__total_subproblem_time
    customers = len(instance.get_demand_customers_id())
    emit(
        status="optimal",
        lp=lp["value"],
        iterations=lp["iterations"],
        columns=len(vrp._VRP__paths) - customers,
        master_s=lp["master"],
        pricing_s=pricing,
        rest_s=solve - lp["master"] - pricing - lp["integer"],
        integer_s=lp["integer"],
    )


def run_cpp_example(name):
    """The current C++ example (rcspp-ng-cg-comparison from the main worktree), ng size 0: cycles allowed."""
    import glob
    import re
    import subprocess

    build = os.path.join(ROOT, "..", "rcspp-main", "build-v2")
    found = glob.glob(os.path.join(build, "**", "rcspp-ng-cg-comparison.exe"), recursive=True)
    if not found:
        sys.exit("rcspp-ng-cg-comparison.exe not built in " + build)
    output = subprocess.run(
        [found[0], "--ng-sizes=0", name], capture_output=True, text=True, cwd=os.path.dirname(found[0])
    ).stdout
    print(output)
    match = re.search(r"lp_cost=(\S+)\s+iterations=(\d+)\s+seconds=(\S+)(.*)", output)
    if not match:
        sys.exit("no result line in the example's output")
    emit(
        status="NotProven" if "NOT proven" in match.group(4) else "Optimal",
        lp=float(match.group(1)),
        iterations=int(match.group(2)),
        solve_s=float(match.group(3)),
    )


def master_model(kind, rows):
    """The user's rows (customer 0 free, customers 1..n equal 1) for each master solver."""
    import rcspp.column_generation as cg

    lower = [-math.inf] + [1.0] * (rows - 1)
    upper = [math.inf] + [1.0] * (rows - 1)
    if kind == "highs":
        return cg.HighsMasterSolver(row_lower=lower, row_upper=upper)
    if kind == "highspy":
        import highspy

        model = highspy.Highs()
        model.setOptionValue("output_flag", False)
        for lo, up in zip(lower, upper):
            model.addRow(lo, up, 0, [], [])
        return cg.HighspyMasterSolver(model)
    if kind == "gurobipy":
        import gurobipy

        model = gurobipy.Model()
        model.Params.OutputFlag = 0
        for lo, up in zip(lower, upper):
            expression = gurobipy.LinExpr()
            if lo == up:
                model.addLConstr(expression, gurobipy.GRB.EQUAL, lo)
            else:
                model.addLConstr(expression, gurobipy.GRB.GREATER_EQUAL, -1e30)
        return cg.GurobipyMasterSolver(model)
    raise ValueError(kind)


def run_layer(name, loop, master_kind, pricing):
    import rcspp.column_generation as cg
    from vrp.vrp import VRP

    class FiniteVRP(VRP):
        def _add_arc(self, graph, orig_id, dest_id, orig, dest):
            if not (orig.depot and dest.depot):  # the example's infinite empty route: not a column
                super()._add_arc(graph, orig_id, dest_id, orig, dest)

    instance = read_instance(name)
    graph = FiniteVRP(instance).construct_resource_graph()  # arcs out of customer i cover row i
    rows = len(instance.get_customers_by_id())  # row 0 is free: the depot has no row
    stages = [cg.PricingStage("simple")]
    if pricing == "heuristic":
        stages = [cg.PricingStage("greedy"), cg.PricingStage("simple")]
    master = cg.MasterProblem(master_model(master_kind, rows))
    master.add_pricer(cg.GraphPricer(graph, stages), cg.ConvexityRow())
    start = time.perf_counter()
    if loop == "cpp":
        result = cg.ColumnGeneration(master).run()
        stats = result.statistics
        fields = dict(
            columns=stats.generated_columns,
            master_s=stats.relaxation_seconds,
            pricing_s=stats.pricing_seconds,
        )
    else:
        import column_generation_loop

        result = column_generation_loop.run(master)
        fields = dict(
            columns=result.generated_columns,
            master_s=result.relaxation_seconds,
            pricing_s=result.pricing_seconds,
        )
    solve = time.perf_counter() - start
    emit(
        status=str(result.status).split(".")[-1],
        lp=result.objective,
        iterations=result.iterations,
        rest_s=solve - fields["master_s"] - fields["pricing_s"],
        **fields,
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("driver", choices=["example", "layer", "cpp"])
    parser.add_argument("--instance", required=True)
    parser.add_argument("--loop", default="cpp", choices=["cpp", "python"])
    parser.add_argument("--master", default="highs", choices=["highs", "highspy", "gurobipy"])
    parser.add_argument("--pricing", default="exact", choices=["exact", "heuristic"])
    args = parser.parse_args()
    if args.driver == "example":
        run_example(args.instance)
    elif args.driver == "cpp":
        run_cpp_example(args.instance)
    else:
        run_layer(args.instance, args.loop, args.master, args.pricing)


if __name__ == "__main__":
    main()
