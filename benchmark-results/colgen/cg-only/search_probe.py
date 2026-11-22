"""Why the layer's search is slower than the example's on the same duals.

The same graph, the same duals, solve() called with each of the layer's arguments in turn.

  python search_probe.py --instance C101_50 --iterations 12 --repeats 5 [--relaxation ng --ng-size 8]

--relaxation adds a fourth resource to the example's graph: ng-routes (each customer remembers its
--ng-size nearest customers, itself included) or elementary routes (every node remembered).
"""

import argparse
import math
import os
import statistics
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import drivers  # noqa: E402


def relaxed_vrp(relaxation, ng_size):
    """The example's VRP with a fourth resource: ng-routes or elementary routes; None: the example's."""
    from rcspp import presets
    from vrp.vrp import VRP

    if relaxation == "none":
        return VRP

    class RelaxedVRP(VRP):
        def _add_nodes_and_arcs(self, graph):
            customers = self._VRP__instance.get_customers_by_id()
            sink = len(customers)

            if relaxation == "elementary":
                presets.add_elementary_resource(graph, "bitset", range(sink + 1))
            else:
                neighborhoods = {sink: {sink}}
                for node_id, node in customers.items():
                    if node.depot:
                        neighborhoods[node_id] = {node_id}
                        continue
                    nearest = sorted(
                        (other_id for other_id, other in customers.items() if not other.depot),
                        key=lambda other_id: self.calculate_distance(node, customers[other_id]),
                    )
                    neighborhoods[node_id] = set(nearest[:ng_size]) | {node_id}
                presets.add_ng_path_resource(graph, "bitset", neighborhoods)

            # The memory ignores its arc value: an empty set after the example's three values.
            add_arc = graph.add_arc
            graph.add_arc = lambda values, *rest: add_arc((*values, set()), *rest)
            try:
                super()._add_nodes_and_arcs(graph)
            finally:
                del graph.add_arc

    return RelaxedVRP


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--instance", required=True)
    parser.add_argument("--iterations", type=int, default=12)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--relaxation", choices=["none", "ng", "elementary"], default="none")
    parser.add_argument("--ng-size", type=int, default=8)
    args = parser.parse_args()
    drivers.check_rcspp_source()

    import vrp.cg.master_problem as master_module
    from rcspp.graph import AlgorithmParams, _algorithm, _chosen_direction

    instance = drivers.read_instance(args.instance)
    example = relaxed_vrp(args.relaxation, args.ng_size)(instance)
    example.generate_initial_paths()
    graph = example._VRP__resource_graph
    master = master_module.MasterProblem(instance.get_demand_customers_id())
    master.add_paths(example._VRP__paths)

    # The example's first iterations: their duals.
    sequence = []
    for _ in range(args.iterations):
        duals = master.solve(relax=True).dual_by_var_id
        sequence.append(duals)
        graph.update_reduced_costs(duals)
        improving = [s for s in graph.solve() if s.cost < -1e-8]
        if not improving:
            break
        master.add_paths(example.add_paths(improving))

    algorithm = _algorithm("simple")
    params = AlgorithmParams()
    direction = _chosen_direction(algorithm, params, None)
    variants = {
        "example: solve()": lambda: graph.solve(),
        "bound -1e-8 only": lambda: graph.solve("simple", -1e-8),
        "params only": lambda: graph.solve("simple", math.inf, AlgorithmParams()),
        "direction only": lambda: graph.solve("simple", math.inf, None, True, 0, direction),
        "layer: all three": lambda: graph.solve(algorithm, -1e-8, params, True, 0, direction),
        "layer, no preprocess": lambda: graph.solve(algorithm, -1e-8, params, False, 0, direction),
    }
    print(f"{args.instance} ({args.relaxation}): {len(sequence)} dual vectors, direction {direction}")
    times = {name: [] for name in variants}
    counts = {}
    for _ in range(args.repeats):
        for name, call in variants.items():
            total, found = 0.0, 0
            for duals in sequence:
                graph.update_reduced_costs(duals)
                start = time.perf_counter()
                result = call()
                total += time.perf_counter() - start
                found += sum(1 for s in result if s.cost < -1e-8)
            times[name].append(total)
            counts[name] = found
    base = statistics.median(times["example: solve()"])
    for name in variants:
        value = statistics.median(times[name])
        print(f"  {name:20s} {1e3 * value / len(sequence):8.3f} ms/solve  x{value / base:.2f}  "
              f"improving paths {counts[name]}")


if __name__ == "__main__":
    main()
