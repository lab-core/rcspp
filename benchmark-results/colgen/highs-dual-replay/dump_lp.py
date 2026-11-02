"""Run the layer (Python loop, HighsMasterSolver) on an instance until the master fails, dump each LP as MPS."""
import os, sys, time
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "benchmark-results", "colgen"))
import drivers  # noqa: sets sys.path
import numpy as np
import highspy
import rcspp.column_generation as cg

OUT = os.getcwd()  # the LPs are written where the script is run from
name = sys.argv[1]
cost = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0

def dump(master, path):
    solver = master.solver()
    ids = np.arange(master.number_of_columns())
    c, lo, up, starts, rows, vals = solver.model_columns(ids)
    rlo, rup, _ = solver.model_rows(0)
    h = highspy.Highs(); h.setOptionValue("output_flag", False)
    h.addRows(len(rlo), rlo, rup, 0, [], [], [])
    h.addCols(len(c), c, lo, up, len(vals), starts[:-1].astype(np.int32), rows.astype(np.int32), vals)
    h.writeModel(path)

from vrp.vrp import VRP
class FiniteVRP(VRP):
    def _add_arc(self, graph, orig_id, dest_id, orig, dest):
        if not (orig.depot and dest.depot):
            super()._add_arc(graph, orig_id, dest_id, orig, dest)

instance = drivers.read_instance(name)
graph = FiniteVRP(instance).construct_resource_graph()
rows = len(instance.get_customers_by_id())
master = cg.MasterProblem(drivers.master_model("highs", rows))
pricer = master.add_pricer(cg.GraphPricer(graph, [cg.PricingStage("simple")]), cg.ConvexityRow())
params = cg.ColumnGenerationParams()
if cost > 0:
    params.initialization = cg.ArtificialColumns(cost=cost)
params.max_iterations = 1
import column_generation_loop
for iteration in range(1000):
    path = os.path.join(OUT, f"{name}_{int(cost)}_it{iteration}.mps")
    if master.initialized():
        dump(master, path)
    t = time.perf_counter()
    result = column_generation_loop.run(master, params)
    print(iteration, result.status, result.objective, master.number_of_columns(), f"{time.perf_counter()-t:.1f}s", flush=True)
    if result.status != cg.ColumnGenerationStatus.IterationLimit:
        break
