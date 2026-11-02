"""A whole layer run (Python loop, HighsMasterSolver, artificial start) with HiGHS options: full.py INSTANCE [opt=val ...]"""
import os, sys, time
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "benchmark-results", "colgen"))
import drivers
import rcspp.column_generation as cg
import column_generation_loop
from vrp.vrp import VRP
class FiniteVRP(VRP):
    def _add_arc(self, graph, orig_id, dest_id, orig, dest):
        if not (orig.depot and dest.depot):
            super()._add_arc(graph, orig_id, dest_id, orig, dest)
name = sys.argv[1]
instance = drivers.read_instance(name)
graph = FiniteVRP(instance).construct_resource_graph()
solver = drivers.master_model("highs", len(instance.get_customers_by_id()))
for arg in sys.argv[2:]:
    k, v = arg.split("=")
    try: v = int(v)
    except ValueError: v = float(v)
    solver.set_option(k, v)
master = cg.MasterProblem(solver)
master.add_pricer(cg.GraphPricer(graph, [cg.PricingStage("simple")]), cg.ConvexityRow())
t = time.perf_counter()
r = column_generation_loop.run(master, cg.ColumnGenerationParams())
print("RUN", name, sys.argv[2:], r.status, r.objective, r.iterations, r.artificial_cost, f"{time.perf_counter()-t:.1f}s", flush=True)
