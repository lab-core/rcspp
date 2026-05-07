#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
from typing import Optional

import networkx as nx

from . import _core as _ext
from .resource import _GenericFunctionDescriptor

# String → Algorithm enum mapping (populated lazily after _ext is imported)
_ALGORITHM_MAP = {
    "simple": lambda: _ext.graph.Algorithm.Simple,
    "pulling": lambda: _ext.graph.Algorithm.Pulling,
    "greedy": lambda: _ext.graph.Algorithm.Greedy,
}

# Kept for backward compatibility
ALGORITHMS = tuple(_ALGORITHM_MAP)

# Ordered resource-type signature → internal C++ class name
_RG_CLASS = {
    ("real",): "_RealResourceGraph",
    ("int",): "_IntResourceGraph",
    ("real", "int"): "_RealIntResourceGraph",
    ("int", "real"): "_RealIntResourceGraph",  # C++ slot order is (real, int)
}


class ResourceGraph:
    """Factory that defers C++ ResourceGraph instantiation until the first graph
    operation, picking the right template from the resources added via
    :meth:`add_real_resource` / :meth:`add_int_resource`."""

    def __init__(self, nx_graph: Optional[nx.DiGraph] = None, **kwargs):
        self._pending: list[tuple] = []  # ('real'|'int', ext, feas, cost, dom)
        self._graph = None  # actual C++ object, created lazily
        if nx_graph is not None:
            self.from_networkx(nx_graph)

    # ── Resource registration ─────────────────────────────────────────────────

    @staticmethod
    def _resolve(fn, resource_type: str):
        """Instantiate a typed C++ function if *fn* is a generic descriptor."""
        if isinstance(fn, _GenericFunctionDescriptor):
            return fn.create(resource_type)
        return fn

    def add_real_resource(
        self, extension_function, feasibility_function, cost_function, dominance_function
    ):
        ext = self._resolve(extension_function, "real")
        feas = self._resolve(feasibility_function, "real")
        cost = self._resolve(cost_function, "real")
        dom = self._resolve(dominance_function, "real")
        if self._graph is not None:
            self._graph.add_real_resource(ext, feas, cost, dom)
        else:
            self._pending.append(("real", ext, feas, cost, dom))

    def add_int_resource(
        self, extension_function, feasibility_function, cost_function, dominance_function
    ):
        ext = self._resolve(extension_function, "int")
        feas = self._resolve(feasibility_function, "int")
        cost = self._resolve(cost_function, "int")
        dom = self._resolve(dominance_function, "int")
        if self._graph is not None:
            self._graph.add_int_resource(ext, feas, cost, dom)
        else:
            self._pending.append(("int", ext, feas, cost, dom))

    # ── Lazy construction ─────────────────────────────────────────────────────

    def _ensure_graph(self):
        if self._graph is not None:
            return
        # Build a de-duplicated ordered tuple of resource types seen
        seen: dict[str, None] = {}
        for r in self._pending:
            seen[r[0]] = None
        types = tuple(seen)
        cls_name = _RG_CLASS.get(types, "_RealResourceGraph")
        self._graph = getattr(_ext.graph, cls_name)()
        for r_type, ext, feas, cost, dom in self._pending:
            if r_type == "real":
                self._graph.add_real_resource(ext, feas, cost, dom)
            else:
                self._graph.add_int_resource(ext, feas, cost, dom)
        self._pending.clear()

    # ── Explicit forwarding for common operations ─────────────────────────────

    def add_node(self, *args, **kwargs):
        self._ensure_graph()
        return self._graph.add_node(*args, **kwargs)

    def add_arc(self, *args, **kwargs):
        self._ensure_graph()
        return self._graph.add_arc(*args, **kwargs)

    def update_arc(self, *args, **kwargs):
        self._ensure_graph()
        return self._graph.update_arc(*args, **kwargs)

    def get_arc(self, arc_id):
        self._ensure_graph()
        return self._graph.get_arc(arc_id)

    def get_node(self, node_id):
        self._ensure_graph()
        return self._graph.get_node(node_id)

    # ── Algorithm dispatch ────────────────────────────────────────────────────

    def solve(
        self,
        algorithm="simple",
        upper_bound: float = math.inf,
        params=None,
        preprocess: bool = True,
        cost_index: int = 0,
    ):
        """Solve the RCSPP.

        Args:
            algorithm: ``Algorithm.Simple`` (default), ``Algorithm.Pulling``,
                ``Algorithm.Greedy``, or the equivalent strings
                ``'simple'``, ``'pulling'``, ``'greedy'``.
            upper_bound: Prune paths with cost ≥ this value.
            params: :class:`AlgorithmParams` (defaults to ``AlgorithmParams()``).
            preprocess: Run preprocessing before solving.
            cost_index: Index of the cost resource.
        """
        if params is None:
            params = _ext.graph.AlgorithmParams()
        self._ensure_graph()
        if isinstance(algorithm, str):
            factory = _ALGORITHM_MAP.get(algorithm)
            if factory is None:
                raise ValueError(
                    f"Unknown algorithm {algorithm!r}. Choose from: {', '.join(ALGORITHMS)}"
                )
            algorithm = factory()
        return self._graph.solve(algorithm, upper_bound, params, preprocess, cost_index)

    # ── Dual-based reduced-cost update ───────────────────────────────────────

    def update_reduced_costs(self, duals, cost_index: int = 0):
        """Recompute arc extender costs from dual values without rebuilding the graph.

        For each arc, computes::

            reduced_cost = arc.cost - sum(row.coefficient * duals[row.index]
                                          for row in arc.dual_rows)

        and writes ``reduced_cost`` to extender resource slot *cost_index*.
        ``arc.cost`` (base cost) is never modified, so repeated calls with
        different duals are correct across iterations.

        Args:
            duals: ``dict`` mapping row-index → dual value **or** a sequence
                   where ``duals[i]`` is the dual for row *i*.  An empty dict
                   is silently ignored.
            cost_index: Resource slot to update (default 0).
        """
        self._ensure_graph()
        if isinstance(duals, dict):
            if not duals:
                return
            max_idx = max(duals.keys())
            duals_list = [duals.get(i, 0.0) for i in range(max_idx + 1)]
        else:
            duals_list = list(duals)
        self._graph.update_reduced_costs(duals_list, cost_index)

    # ── Transparent delegation for everything else ────────────────────────────

    def __getattr__(self, name: str):
        # Called only when normal lookup fails — forwards C++ graph methods.
        self._ensure_graph()
        return getattr(self._graph, name)

    # ── NetworkX integration ──────────────────────────────────────────────────

    def from_networkx(self, nx_graph: nx.DiGraph):
        for node_id, data in nx_graph.nodes(data=True):
            source = data.get("source", False) is True
            sink = data.get("sink", False) is True
            self.add_node(int(node_id), source, sink)

        for u, v, data in nx_graph.edges(data=True):
            resource_init = None
            if "resource" in data:
                resource_init = ([(res,) for res in data["resource"]],)

            arc_id = data.get("id")
            cost = data.get("cost", 0.0)
            dual_rows = data.get("dual_rows", [])

            if resource_init is not None:
                self.add_arc(resource_init, int(u), int(v), arc_id, cost, dual_rows)
            else:
                self._ensure_graph()
                self._graph.add_arc(int(u), int(v), arc_id, cost, dual_rows)


# Re-export all public graph submodule symbols (Row, AlgorithmParams, Solution, …)
for _k in dir(_ext.graph):
    if not _k.startswith("_") and _k != "ResourceGraph":
        globals()[_k] = getattr(_ext.graph, _k)

# Patch the C++ submodule so `from rcspp._core.graph import ResourceGraph` resolves correctly
_ext.graph.ResourceGraph = ResourceGraph
