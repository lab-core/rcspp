"""The Python example's loop, and at each of its iterations, outside its timers, the pure layer's pricing step on the
same duals: GraphPricer.price and MasterProblem.add_columns into a shadow master (gurobipy, never solved).

  python replay.py --instance C101_25      # prints one RESULT line

Run with the environment of run.py (COLGEN_SOURCE_DIRECTORY, PYTHONPATH). Each side's time is split into the
search (ResourceGraph.solve), the arcs' reduced costs (update_reduced_costs), the LP solver's own column adds
(python-mip's add_var, gurobipy through GurobipyMasterSolver.add_columns) and the rest: the column generation's own
work (paths into columns, de-duplication, bookkeeping).
"""

import argparse
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import drivers  # noqa: E402  (sets the source checkout's paths)

EPSILON = 1e-8  # the example's threshold; the layer is asked for the same


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--instance", required=True)
    args = parser.parse_args()
    drivers.check_rcspp_source()

    import numpy as np
    import rcspp.column_generation as cg
    import vrp.cg.master_problem as master_module
    from rcspp.column_generation import PricingRequest
    from vrp.vrp import VRP

    timed = drivers.time_engine()
    timed(master_module.MasterProblem, "_add_column", "solver_add_s")
    timed(cg.GurobipyMasterSolver, "add_columns", "solver_add_s")
    engine = drivers.ENGINE

    class FiniteVRP(VRP):
        def _add_arc(self, graph, orig_id, dest_id, orig, dest):
            if not (orig.depot and dest.depot):
                super()._add_arc(graph, orig_id, dest_id, orig, dest)

    instance = drivers.read_instance(args.instance)

    # The example, as VRP.solve builds it.
    example = VRP(instance)
    example.generate_initial_paths()
    graph = example._VRP__resource_graph
    master = master_module.MasterProblem(instance.get_demand_customers_id())
    master.add_paths(example._VRP__paths)

    # The layer's shadow, as drivers.run_layer builds it.
    rows = len(instance.get_customers_by_id())
    layer_master = cg.MasterProblem(drivers.master_model(cg, "gurobipy", rows))
    pricer = cg.GraphPricer(FiniteVRP(instance).construct_resource_graph(), [cg.PricingStage("simple")])
    index = layer_master.add_pricer(pricer, cg.ConvexityRow())
    drivers.add_single_customer_routes(layer_master, index, instance)

    parts = ("total", "search_s", "reduced_costs_s", "solver_add_s")
    sides = {side: dict.fromkeys(parts, 0.0) for side in ("old", "layer")}
    master_s = 0.0
    iterations = columns = differing = 0
    best_gap = 0.0
    min_reduced_cost = -math.inf

    def measured(side, work):
        before = dict(engine)
        start = time.perf_counter()
        result = work()
        sides[side]["total"] += time.perf_counter() - start
        for key in parts[1:]:
            sides[side][key] += engine[key] - before[key]
        return result

    while min_reduced_cost < -EPSILON:
        start = time.perf_counter()
        solution = master.solve(relax=True)
        master_s += time.perf_counter() - start
        duals = solution.dual_by_var_id
        iterations += 1

        # The example's pricing step, as VRP.solve and VRP.solve_subproblem do it.
        def old_step():
            graph.update_reduced_costs(duals)
            solutions = graph.solve()
            improving = [s for s in solutions if s.cost < -EPSILON]
            master.add_paths(example.add_paths(improving))
            return solutions, improving

        solutions, improving = measured("old", old_step)
        min_reduced_cost = min((s.cost for s in solutions), default=math.inf)

        # The layer's pricing step on the same duals, as ColumnGeneration._price does it.
        vector = np.zeros(layer_master.number_of_rows)
        for row, value in duals.items():
            vector[row] = value

        def layer_step():
            priced = pricer.price(vector, PricingRequest(reduced_cost_threshold=-EPSILON))
            layer_master.add_columns(index, priced.columns)
            return priced.columns

        found = measured("layer", layer_step)
        columns += len(improving)
        old_paths = sorted(tuple(s.path_node_ids) for s in improving)
        new_paths = sorted(tuple(s.path_node_ids) for s in found)
        if old_paths != new_paths:
            differing += 1
        if improving and found:
            best_gap = max(best_gap, abs(min(s.cost for s in improving) - min(s.cost for s in found)))

    print(
        "RESULT "
        + json.dumps(
            dict(
                lp=solution.cost,
                iterations=iterations,
                columns=columns,
                layer_columns=layer_master.number_of_columns - len(instance.get_demand_customers_id()),
                differing_iterations=differing,
                best_cost_gap=best_gap,
                master_s=master_s,
                old=sides["old"],
                layer=sides["layer"],
            )
        ),
        flush=True,
    )


if __name__ == "__main__":
    main()
