#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
from typing import Optional

import networkx as nx

from . import _core as _ext
from ._resource_types import ALIASES, ALL, CPP_NAME, FULL, FULL_CLASS, MIXED, canonical
from .resource import _GenericFunctionDescriptor

# String → Algorithm enum mapping (populated lazily after _ext is imported)
_ALGORITHM_MAP = {
    "simple": lambda: _ext.graph.Algorithm.Simple,
    "pulling": lambda: _ext.graph.Algorithm.Pulling,
    "greedy": lambda: _ext.graph.Algorithm.Greedy,
}

# Kept for backward compatibility
ALGORITHMS = tuple(_ALGORITHM_MAP)

# Canonical resource type names exposed through add_<type>_resource methods.
_ALL_RESOURCE_TYPES = ALL


def _rg_class_name(*cpp_types: str) -> str:
    """Derive the C++ binding class name from C++ type name(s)."""
    return "_" + "_".join(cpp_types) + "_resource_graph"


# ── Type-signature → C++ class-name lookup table ─────────────────────────────
# Keys are tuples of canonical Python type names; values are C++ binding class names.
# Only combinations that are actually compiled in C++ are registered
# (verified via hasattr at module load time).

_RG_CLASS: dict[tuple[str, ...], str] = {}

# Single-resource graphs
for _rt in ALL:
    _cpp = CPP_NAME.get(_rt, _rt)
    _cls = _rg_class_name(_cpp)
    if hasattr(_ext.graph, _cls):
        _RG_CLASS[(_rt,)] = _cls

# Mixed pairs — auto-generated; permutations of a pair map to the same C++ class.
for _combo in MIXED:
    _cpp_combo = tuple(CPP_NAME.get(t, t) for t in _combo)
    _cls = _rg_class_name(*_cpp_combo)
    if hasattr(_ext.graph, _cls):
        _RG_CLASS[_combo] = _cls
        if len(_combo) == 2 and _combo[::-1] != _combo:
            _RG_CLASS[_combo[::-1]] = _cls

# Universal (all-types) graph — always present as the catch-all fallback.
if hasattr(_ext.graph, FULL_CLASS):
    _RG_CLASS[FULL] = FULL_CLASS


class ResourceGraph:
    """Factory that defers C++ ResourceGraph instantiation until the first graph
    operation, picking the right template from the resources added via
    :meth:`add_real_resource` / :meth:`add_int_resource` / etc."""

    def __init__(self, nx_graph: Optional[nx.DiGraph] = None, **kwargs):
        self._pending: list[tuple] = []  # (canonical_type, ext, feas, cost, dom)
        self._graph = None  # actual C++ object, created lazily
        # Set after _ensure_graph(): the canonical slot tuple of the C++ class
        # and the canonical tuple of user-registered types (may differ when
        # a superset graph is chosen as fallback).
        self._graph_canonical: tuple[str, ...] = ()
        self._registered_canonical: tuple[str, ...] = ()
        if nx_graph is not None:
            self.from_networkx(nx_graph)

    # ── Resource registration ─────────────────────────────────────────────────

    @staticmethod
    def _resolve(fn, canonical_type: str):
        """Instantiate a typed C++ function object if *fn* is a generic descriptor.

        ``canonical_type`` is translated to the C++ prefix via ``CPP_NAME``
        before the C++ class is looked up.
        """
        if isinstance(fn, _GenericFunctionDescriptor):
            cpp_type = CPP_NAME.get(canonical_type, canonical_type)
            return fn.create(cpp_type)
        return fn

    # ── Lazy construction ─────────────────────────────────────────────────────

    def _ensure_graph(self):
        if self._graph is not None:
            return

        seen: dict[str, None] = {}
        for r in self._pending:
            seen[r[0]] = None
        requested = frozenset(seen)
        types = canonical(*requested)

        cls_name = _RG_CLASS.get(types)
        selected_combo = types

        if cls_name is None:
            # Find the smallest C++ class that is a superset of the requested types.
            candidates = sorted(
                ((combo, cls) for combo, cls in _RG_CLASS.items() if requested <= frozenset(combo)),
                key=lambda x: len(x[0]),
            )
            if not candidates:
                raise ValueError(
                    f"No C++ ResourceGraph is bound for resource combination {types!r}. "
                    f"Available combinations: {sorted(_RG_CLASS)}"
                )
            selected_combo, cls_name = candidates[0]

        self._graph_canonical = selected_combo
        self._registered_canonical = types

        self._graph = getattr(_ext.graph, cls_name)()
        for r_type, ext, feas, cost, dom in self._pending:
            cpp_name = CPP_NAME.get(r_type, r_type)
            getattr(self._graph, f"add_{cpp_name}_resource")(ext, feas, cost, dom)
        self._pending.clear()

    # ── Consumption-tuple expansion ───────────────────────────────────────────

    def _expand_consumption(self, consumption: tuple) -> tuple:
        """Expand a partial resource-consumption tuple to the full graph's N-slot tuple.

        When using a superset graph (e.g. _all_resource_graph for a real+int_set
        problem), the user passes a K-element tuple for K registered types.  This
        expands it to the N-element tuple the C++ graph expects, inserting empty lists
        for unregistered slots.
        """
        graph_types = self._graph_canonical
        reg_types = self._registered_canonical
        result: list = [[] for _ in graph_types]
        for i, rt in enumerate(reg_types):
            slot = graph_types.index(rt)
            if i < len(consumption):
                result[slot] = consumption[i]
        return tuple(result)

    @property
    def _needs_expansion(self) -> bool:
        return self._graph_canonical != self._registered_canonical

    # ── Explicit forwarding for common operations ─────────────────────────────

    def add_node(self, *args, **kwargs):
        self._ensure_graph()
        return self._graph.add_node(*args, **kwargs)

    def add_arc(self, first, *args, **kwargs):
        self._ensure_graph()
        # Expand partial consumption tuples when a superset graph is used.
        # A tuple/list first argument is a resource-consumption; int/Node is an origin id.
        if self._needs_expansion and isinstance(first, (tuple, list)):
            first = self._expand_consumption(first)
        return self._graph.add_arc(first, *args, **kwargs)

    def update_arc(self, arc, resource_consumption, *args, **kwargs):
        self._ensure_graph()
        if self._needs_expansion:
            resource_consumption = self._expand_consumption(resource_consumption)
        return self._graph.update_arc(arc, resource_consumption, *args, **kwargs)

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


# ── Generate add_<type>_resource methods ─────────────────────────────────────


def _make_add_resource_method(canonical_type: str):
    def add_resource_method(
        self, extension_function, feasibility_function, cost_function, dominance_function
    ):
        if self._graph is not None:
            raise RuntimeError(
                f"Cannot call add_{canonical_type}_resource after the graph has been "
                "initialized (i.e. after the first add_node / add_arc / solve call). "
                "Add all resources before performing any graph operation."
            )
        ext = self._resolve(extension_function, canonical_type)
        feas = self._resolve(feasibility_function, canonical_type)
        cost = self._resolve(cost_function, canonical_type)
        dom = self._resolve(dominance_function, canonical_type)
        self._pending.append((canonical_type, ext, feas, cost, dom))

    add_resource_method.__name__ = f"add_{canonical_type}_resource"
    return add_resource_method


for _rt in _ALL_RESOURCE_TYPES:
    setattr(ResourceGraph, f"add_{_rt}_resource", _make_add_resource_method(_rt))

# Backward-compat: old C++-flavoured names delegate to the canonical method.
for _alias, _canonical_type in ALIASES.items():
    setattr(ResourceGraph, f"add_{_alias}_resource", _make_add_resource_method(_canonical_type))


# Re-export all public graph submodule symbols (Row, AlgorithmParams, Solution, …)
for _k in dir(_ext.graph):
    if not _k.startswith("_") and _k != "ResourceGraph":
        globals()[_k] = getattr(_ext.graph, _k)

# Patch the C++ submodule so `from rcspp._core.graph import ResourceGraph` resolves correctly
_ext.graph.ResourceGraph = ResourceGraph
