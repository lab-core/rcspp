#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
from typing import Optional

import networkx as nx

from . import _core as _ext
from ._resource_types import ALL, CPP_NAME, canonical
from .resource import _GenericFunctionDescriptor

# String → Algorithm enum mapping (populated lazily after _ext is imported)
_ALGORITHM_MAP = {
    "simple": lambda: _ext.graph.Algorithm.Simple,
    "pushing": lambda: _ext.graph.Algorithm.Pushing,
    "pulling": lambda: _ext.graph.Algorithm.Pulling,
    "greedy": lambda: _ext.graph.Algorithm.Greedy,
}

# Kept for backward compatibility
ALGORITHMS = tuple(_ALGORITHM_MAP)

# Canonical resource type names exposed through add_<type>_resource methods.
_ALL_RESOURCE_TYPES = ALL

# ── Type-signature → C++ class-name lookup table ─────────────────────────────
# Built by scanning _ext.graph for _*_resource_graph classes and parsing their
# names back to canonical Python-type tuples.  Adding a new combination to
# graph.cpp automatically makes it available here — no Python changes needed.

# Reverse of CPP_NAME: C++ prefix → Python name (e.g. "uint_bitset" → "bitset").
# Types without a CPP_NAME entry map to themselves.
_CPP_TO_PY: dict[str, str] = {cpp: py for py, cpp in CPP_NAME.items()}
# Sorted longest-first so the greedy parser never confuses "real" with "real_set".
_CPP_NAMES: list[str] = sorted([CPP_NAME.get(t, t) for t in ALL], key=len, reverse=True)


def _parse_rg_class(attr: str) -> tuple[str, ...] | None:
    """Parse a _*_resource_graph attribute name into a canonical Python-type tuple.

    Returns None when the name contains a C++ type not in the Python registry
    """
    suffix = "_resource_graph"
    if not (attr.startswith("_") and attr.endswith(suffix)):
        return None
    inner = attr[1 : -len(suffix)]
    py_types: list[str] = []
    while inner:
        for cpp in _CPP_NAMES:
            if inner == cpp:
                py_types.append(_CPP_TO_PY.get(cpp, cpp))
                inner = ""
                break
            if inner.startswith(cpp + "_"):
                py_types.append(_CPP_TO_PY.get(cpp, cpp))
                inner = inner[len(cpp) + 1 :]
                break
        else:
            return None
    return canonical(*py_types) if py_types else None


_RG_CLASS: dict[tuple[str, ...], str] = {}

_py_type_set = set(ALL)
for _attr in dir(_ext.graph):
    _types = _parse_rg_class(_attr)
    if _types is not None and all(t in _py_type_set for t in _types):
        _RG_CLASS[_types] = _attr


class ResourceGraph:
    """Factory that defers C++ ResourceGraph instantiation until the first graph
    operation, picking the right template from the resources added via
    :meth:`add_real_resource` / :meth:`add_int_resource` / etc."""

    def __init__(self, nx_graph: Optional[nx.DiGraph] = None, **kwargs):
        self._pending: list[tuple] = []  # (canonical_type, ext, feas, cost, dom)
        self._refs: list = []  # keep Python wrappers alive against GC
        self._graph = None  # actual C++ object, created lazily
        # Set after _ensure_graph():
        #   _graph_canonical    – full slot tuple of the chosen C++ class (canonical order)
        #   _registered_order   – types in the order the user called add_<type>_resource()
        self._graph_canonical: tuple[str, ...] = ()
        self._registered_order: tuple[str, ...] = ()
        self._full_registration_order: list[str] = []  # per-instance, incl. duplicates
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

        # Preserve registration order (first occurrence of each type in _pending).
        reg_order: list[str] = []
        seen_set: set[str] = set()
        for r in self._pending:
            if r[0] not in seen_set:
                reg_order.append(r[0])
                seen_set.add(r[0])

        full_reg_order = [r[0] for r in self._pending]

        # The first registered resource must be a real-valued cost resource.
        if not full_reg_order:
            raise ValueError("At least one resource must be registered before using the graph.")
        if full_reg_order[0] != "real":
            raise ValueError(
                f"The first registered resource must be a real resource for the cost, "
                f"got {full_reg_order[0]!r}. Register a real resource before any other type."
            )

        requested = frozenset(reg_order)
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
        self._registered_order = tuple(reg_order)
        self._full_registration_order = full_reg_order

        self._graph = getattr(_ext.graph, cls_name)()
        for r_type, ext, feas, cost, dom in self._pending:
            cpp_name = CPP_NAME.get(r_type, r_type)
            getattr(self._graph, f"add_{cpp_name}_resource")(ext, feas, cost, dom)
            self._refs.extend([ext, feas, cost, dom])
        self._pending.clear()

    # ── Explicit forwarding for common operations ─────────────────────────────

    def _normalize_consumption(self, consumption):
        """Normalise a resource-consumption argument into canonical-ordered per-type
        lists.

        Returns a tuple with one list per canonical C++ slot, ready to pass to C++.
        Reordering is always applied.

        **Flat style** — one element per registered *instance* in the exact order
        ``add_<type>_resource`` was called (duplicates included).  A tuple value is
        passed as-is (multi-component initialiser); any other value is wrapped in a
        1-tuple::

            (1, 5.0, (3, 5), {4})
            # add_int / add_real / add_int / add_set were called in that order
            # → ([(1,), (3,5)], [(5.0,)], [({4},)]) after canonical reordering
        """
        if not isinstance(consumption, tuple):
            consumption = (consumption,)
        if len(consumption) != len(self._full_registration_order):
            raise ValueError(
                f"Expected {len(self._full_registration_order)} resource components in "
                f"consumption, got {len(consumption)}. The number of components must "
                f"match the number of registered resources (including duplicates) and "
                f"their order must match the order in which add_<type>_resource() was "
                f"called."
            )
        result: list = [[] for _ in self._graph_canonical]
        for i, item in enumerate(consumption):
            rt = self._full_registration_order[i]
            slot = self._graph_canonical.index(rt)
            result[slot].append(item if isinstance(item, tuple) else (item,))
        return tuple(result)

    def add_node(self, *args, **kwargs):
        self._ensure_graph()
        return self._graph.add_node(*args, **kwargs)

    def add_arc(self, resource_consumption, *args, **kwargs):
        self._ensure_graph()
        norm_res_cons = self._normalize_consumption(resource_consumption)
        return self._graph.add_arc(norm_res_cons, *args, **kwargs)

    def update_arc(self, arc, resource_consumption, *args, **kwargs):
        self._ensure_graph()
        norm_res_cons = self._normalize_consumption(resource_consumption)
        return self._graph.update_arc(arc, norm_res_cons, *args, **kwargs)

    def get_arc(self, arc_id):
        self._ensure_graph()
        return self._graph.get_arc(arc_id)

    def get_node(self, node_id):
        self._ensure_graph()
        return self._graph.get_node(node_id)

    def sort_nodes(self, comp=None):
        """Sort graph nodes in place.

        Args:
            comp: Optional callable ``(node1, node2) -> bool`` returning True when
                *node1* should come before *node2*.  When omitted, nodes are sorted
                by ascending ``node.id``.
        """
        self._ensure_graph()
        if comp is None:
            self._graph.sort_nodes()
        else:
            self._graph.sort_nodes(comp)

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
            algorithm: ``Algorithm.Simple`` (default), ``Algorithm.Pushing``,
                ``Algorithm.Pulling``, ``Algorithm.Greedy``, or the equivalent strings
                ``'simple'``, ``'pushing'``, `'pulling'``, ``'greedy'``.
            upper_bound: Prune paths with cost ≥ this value.
            params: :class:`AlgorithmParams` (defaults to ``AlgorithmParams()``).
            preprocess: Run preprocessing before solving.
            cost_index: Index within the cost resource type (the first ``real``
                or ``int`` slot in canonical order that the user registered).
                Defaults to 0.
        """
        _ext.graph.check_interrupted()
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
            duals: ``dict`` mapping row-index → dual value, **or** a ``list``
                   (or any sequence) where ``duals[i]`` is the dual for row *i*.
                   An empty dict is silently ignored.
            cost_index: Resource slot to update (default 0).
        """
        self._ensure_graph()
        if isinstance(duals, dict):
            if not duals:
                return  # honor the docstring: empty dict ⇒ leave reduced costs unchanged
            max_idx = max(duals.keys())
            duals_list = [duals.get(i, 0.0) for i in range(max_idx + 1)]
        else:
            duals_list = list(duals)
        self._graph.update_reduced_costs(duals_list, cost_index)

    # ── String representation ─────────────────────────────────────────────────

    def to_string(self, print_arcs: bool = True) -> str:
        if self._graph is None:
            return ""
        return self._graph.to_string(print_arcs)

    def __str__(self) -> str:
        return self.to_string()

    def __repr__(self) -> str:
        return self.to_string()

    # ── Transparent delegation for everything else ────────────────────────────

    def __getattr__(self, name: str):
        # Called only when normal lookup fails — forwards C++ graph methods.
        self._ensure_graph()
        return getattr(self._graph, name)

    # ── NetworkX integration ──────────────────────────────────────────────────

    def from_networkx(self, nx_graph: nx.DiGraph):
        # ── Structural validation ─────────────────────────────────────────────
        source_nodes = [n for n, d in nx_graph.nodes(data=True) if d.get("source") is True]
        sink_nodes = [n for n, d in nx_graph.nodes(data=True) if d.get("sink") is True]
        if not source_nodes:
            raise ValueError(
                "NetworkX graph has no source node. "
                "Set source=True on at least one node:\n"
                "  G.nodes[node_id]['source'] = True\n"
                "  or G.add_node(node_id, source=True)"
            )
        if not sink_nodes:
            raise ValueError(
                "NetworkX graph has no sink node. "
                "Set sink=True on at least one node:\n"
                "  G.nodes[node_id]['sink'] = True\n"
                "  or G.add_node(node_id, sink=True)"
            )

        # ── Arc resource validation ───────────────────────────────────────────
        # If resources have been registered every arc must carry a 'resource' tuple;
        # otherwise the C++ binding would be called with the wrong argument types.
        resources_registered = bool(self._pending) or (self._graph is not None)
        if resources_registered:
            missing = [(u, v) for u, v, d in nx_graph.edges(data=True) if "resource" not in d]
            if missing:
                n_res = len(self._pending) or len(self._full_registration_order)
                pairs = ", ".join(f"({u} → {v})" for u, v in missing[:5])
                if len(missing) > 5:
                    pairs += f" … ({len(missing) - 5} more)"
                raise ValueError(
                    f"{len(missing)} arc(s) are missing a 'resource' attribute, "
                    f"but {n_res} resource(s) are registered: {pairs}.\n"
                    f"Provide resource consumption for every arc:\n"
                    f"  G.add_edge(u, v, resource=(val1, val2, ...))"
                )

        # ── Build graph ───────────────────────────────────────────────────────
        for node_id, data in nx_graph.nodes(data=True):
            source = data.get("source", False) is True
            sink = data.get("sink", False) is True
            self.add_node(int(node_id), source, sink)

        for u, v, data in nx_graph.edges(data=True):
            if "resource" not in data:
                raise ValueError(
                    f"Arc ({u} → {v}) is missing 'resource' attribute. Provide resource "
                    f"consumption for every arc:\n  G.add_edge(u, v, resource=(val1, val2, ...))"
                )

            resource_init = tuple(data["resource"])
            arc_id = data.get("id")
            cost = data.get("cost", 0.0)
            dual_rows = data.get("dual_rows", [])
            self.add_arc(resource_init, int(u), int(v), cost, dual_rows, arc_id)


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
        if len(self._pending) == 0 and canonical_type != "real":
            raise ValueError(
                f"The first registered resource must be a cost resource ('real'), "
                f"got {canonical_type!r}. Register a real resource before any other type."
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


# Re-export all public graph submodule symbols (Row, AlgorithmParams, Solution, …)
for _k in dir(_ext.graph):
    if not _k.startswith("_") and _k != "ResourceGraph":
        globals()[_k] = getattr(_ext.graph, _k)

# Patch the C++ submodule so `from rcspp._core.graph import ResourceGraph` resolves correctly
_ext.graph.ResourceGraph = ResourceGraph
