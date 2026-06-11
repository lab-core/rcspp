#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Resource-constrained shortest-path graph abstraction.

Provides the Python-side :class:`ResourceGraph` factory and the
:class:`BucketAlgorithmParams` helper.  The heavy lifting is done by a
generated C++ extension (``rcspp._core.graph``); this module handles lazy
construction, resource registration, buffered node/arc insertion, and
NetworkX integration.
"""

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
    "tabu": lambda: _ext.graph.Algorithm.Tabu,
    "astar": lambda: _ext.graph.Algorithm.AStar,
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
    :meth:`add_real_resource` / :meth:`add_int_resource` / etc.

    Nodes and arcs added via :meth:`add_node` / :meth:`add_arc` are buffered in
    Python and sent to C++ in one batch when the graph is first used (solve,
    get_node, get_arc, …) or when :meth:`update` is called explicitly.  This
    eliminates per-call Python→C++ overhead and allows the C++ hash-maps to be
    pre-allocated with a single ``reserve`` call.
    """

    def __init__(self, nx_graph: Optional[nx.DiGraph] = None, **kwargs):
        """Initialise a ResourceGraph, optionally from a NetworkX DiGraph.

        Args:
            nx_graph: Optional directed graph whose nodes and arcs are
                imported immediately via :meth:`from_networkx`.
            **kwargs: Reserved for future use; currently unused.
        """
        self._pending: list[tuple] = []  # (canonical_type, ext, feas, cost, dom)
        self._refs: list = []  # keep Python wrappers alive against GC
        self._graph = None  # actual C++ object, created lazily
        # Set after _ensure_graph():
        #   _graph_canonical    – full slot tuple of the chosen C++ class (canonical order)
        #   _registered_order   – types in the order the user called add_<type>_resource()
        self._graph_canonical: tuple[str, ...] = ()
        self._registered_order: tuple[str, ...] = ()
        self._full_registration_order: list[str] = []  # per-instance, incl. duplicates
        # Buffers for deferred node/arc insertion
        self._node_buffer: list = []  # list of (id, source, sink)
        self._arc_buffer: list = []  # list of (raw_consumption, origin, dest, cost, rows)
        self._rows_buffer: list = []  # list of (arc_id, row_index, coeff) triples
        self._reserve_hint: tuple[int, int] = (
            0,
            0,
        )  # (n_nodes, n_arcs) hint from reserve()
        self._next_arc_id: int = 0  # mirrors C++ next_arc_id_; returned by add_arc
        if nx_graph is not None:
            self.from_networkx(nx_graph)

    # ── Resource registration ─────────────────────────────────────────────────

    @staticmethod
    def _resolve(fn, canonical_type: str):
        """Instantiate a typed C++ function object if *fn* is a generic descriptor.

        ``canonical_type`` is translated to the C++ prefix via ``CPP_NAME`` before the
        C++ class is looked up.
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
        if full_reg_order[0] != "real":  # pragma: no cover
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
            if not candidates:  # pragma: no cover
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

    def _flush(self):
        """Flush buffered nodes and arcs to C++, creating the graph if needed.

        Calls ``reserve`` once with the total expected counts so the C++ hash-maps
        are allocated in a single shot instead of rehashing on every insert.
        """
        self._ensure_graph()
        if not self._node_buffer and not self._arc_buffer and not self._rows_buffer:
            return
        n_nodes = max(
            self._reserve_hint[0],
            self._graph.number_of_nodes() + len(self._node_buffer),
        )
        n_arcs = max(self._reserve_hint[1], self._graph.number_of_arcs() + len(self._arc_buffer))
        self._graph.reserve(n_nodes, n_arcs)
        self._reserve_hint = (0, 0)
        if self._node_buffer:
            self._graph._add_nodes_bulk(self._node_buffer)
            self._node_buffer.clear()
        if self._arc_buffer:
            consumptions, origins, dests, costs, all_rows = [], [], [], [], []
            for raw_cons, origin_id, dest_id, cost, rows in self._arc_buffer:
                consumptions.append(self._normalize_consumption(raw_cons))
                origins.append(origin_id)
                dests.append(dest_id)
                costs.append(cost)
                all_rows.append(rows)
            self._arc_buffer.clear()
            self._graph._add_arcs_bulk(consumptions, origins, dests, costs, all_rows)
        if self._rows_buffer:
            import numpy as np

            arr = np.array(self._rows_buffer, dtype=np.float64)
            self._graph._add_rows_bulk(arr)
            self._rows_buffer.clear()

    # ── Normalisation helper ──────────────────────────────────────────────────

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

    @staticmethod
    def _normalize_rows(rows) -> list:
        """Normalise rows to a list of Row objects.

        Args:
            rows: None, a single ``(index, coeff)`` tuple, a list of such tuples,
                  or a list of :class:`Row` objects (passed through unchanged).

        Returns:
            A list of :class:`Row` objects.
        """
        if rows is None:
            return []
        _Row = _ext.graph.Row
        if isinstance(rows, tuple) and len(rows) == 2:
            return [_Row(int(rows[0]), float(rows[1]))]
        result = []
        for r in rows:
            result.append(_Row(int(r[0]), float(r[1])) if isinstance(r, tuple) else r)
        return result

    # ── Graph mutation (buffered) ─────────────────────────────────────────────

    def add_node(self, node_id: int, source: bool = False, sink: bool = False):
        """Buffer a node for insertion.

        The node is not sent to C++ immediately; call :meth:`update` or any read
        operation to flush the buffer.
        """
        self._node_buffer.append((int(node_id), bool(source), bool(sink)))

    def add_arc(
        self,
        resource_consumption,
        origin_id: int,
        destination_id: int,
        cost: float = 0.0,
        rows=None,
    ):
        """Buffer an arc for insertion.

        The arc is not sent to C++ immediately; call :meth:`update` or any read
        operation to flush the buffer.  Arc resource normalization happens at flush
        time so resources must be registered before :meth:`update` is called.
        """
        rows = self._normalize_rows(rows)
        arc_id = self._next_arc_id
        self._next_arc_id += 1
        self._arc_buffer.append(
            (
                resource_consumption,
                int(origin_id),
                int(destination_id),
                float(cost),
                rows,
            )
        )
        return arc_id

    def remove_arcs(self, arc_ids):
        """Remove a batch of arcs by id.

        Args:
            arc_ids: list of arc ids **or** a 1-D numpy integer array.

        Returns:
            List of ids that were actually removed (ids not found in the graph
            are silently skipped).
        """
        self._flush()
        if hasattr(arc_ids, "tolist"):
            arc_ids = arc_ids.tolist()
        return self._graph.remove_arcs(arc_ids)

    def restore_arcs(self, arc_ids):
        """Restore a batch of previously removed arcs by id.

        Args:
            arc_ids: list of arc ids **or** a 1-D numpy integer array.

        Returns:
            List of ids that were actually restored (ids not in the removed-arc
            pool are silently skipped).
        """
        self._flush()
        if hasattr(arc_ids, "tolist"):
            arc_ids = arc_ids.tolist()
        return self._graph.restore_arcs(arc_ids)

    def update(self):
        """Flush all buffered nodes and arcs to the C++ graph.

        Call this explicitly after bulk insertions if you need the graph to be
        up-to-date before inspecting it (e.g. ``get_node``, ``get_arc``, …).
        All read operations flush the buffer automatically, so this call is
        optional but can be useful for explicit control.
        """
        self._flush()

    def reserve(self, n_nodes: int, n_arcs: int):
        """Pre-allocate capacity for *n_nodes* nodes and *n_arcs* arcs.

        When called before the graph has been flushed, the hint is stored and applied at
        flush time so the C++ hash-maps are allocated in one shot. When called after the
        graph is already populated, the reservation is forwarded to C++ immediately.
        """
        if self._graph is not None:
            self._graph.reserve(n_nodes, n_arcs)
        else:
            self._reserve_hint = (
                max(self._reserve_hint[0], n_nodes),
                max(self._reserve_hint[1], n_arcs),
            )

    # ── Graph size (no flush required — includes buffered counts) ─────────────

    def get_nodes_size(self) -> int:
        """Return the total number of nodes, including those still in the buffer."""
        n = len(self._node_buffer)
        if self._graph is not None:
            n += self._graph.number_of_nodes()
        return n

    def get_arcs_size(self) -> int:
        """Return the total number of arcs, including those still in the buffer."""
        n = len(self._arc_buffer)
        if self._graph is not None:
            n += self._graph.number_of_arcs()
        return n

    # ── Graph read operations (flush first) ───────────────────────────────────

    def get_node(self, node_id):
        """Return the node with the given ID, flushing pending buffers first.

        Args:
            node_id: Integer node identifier.

        Returns:
            The C++ node object for *node_id*.
        """
        self._flush()
        return self._graph.get_node(node_id)

    def get_arc(self, arc_id):
        """Return the arc with the given ID, flushing pending buffers first.

        Args:
            arc_id: Integer arc identifier.

        Returns:
            The C++ arc object for *arc_id*.
        """
        self._flush()
        return self._graph.get_arc(arc_id)

    def get_arcs(self, origin_id: int, destination_id: int):
        """Return all arcs between *origin_id* and *destination_id* as a list."""
        self._flush()
        return self._graph.get_arcs(origin_id, destination_id)

    def update_arc(self, arc, resource_consumption, *args, **kwargs):
        """Update an arc's resource consumption in place.

        Flushes all pending buffers before modifying the arc so the C++ object
        is fully built.  The *resource_consumption* argument follows the same
        flat-style convention as :meth:`add_arc`.

        Args:
            arc: Arc object or arc ID to update.
            resource_consumption: New resource consumption values, in the same
                order as the original :meth:`add_arc` call.
            *args: Extra positional arguments forwarded to the C++ ``update_arc``.
            **kwargs: Extra keyword arguments forwarded to the C++ ``update_arc``.

        Returns:
            Whatever the C++ ``update_arc`` returns.
        """
        self._flush()
        norm_res_cons = self._normalize_consumption(resource_consumption)
        return self._graph.update_arc(arc, norm_res_cons, *args, **kwargs)

    def add_rows_to_arc(self, arc_id: int, rows) -> None:
        """Buffer rows to be appended to arc *arc_id*; applied at next flush/solve.

        Args:
            arc_id: Target arc ID.
            rows: Single ``(index, coeff)`` tuple, a list of such tuples, or a
                  list of :class:`Row` objects.
        """
        arc_id = int(arc_id)
        if isinstance(rows, tuple) and len(rows) == 2:
            rows = [rows]
        for r in rows:
            if isinstance(r, tuple):
                self._rows_buffer.append((arc_id, int(r[0]), float(r[1])))
            else:
                self._rows_buffer.append((arc_id, int(r.index), float(r.coefficient)))

    def add_rows(self, data) -> None:
        """Buffer rows for multiple arcs at once; applied at next flush/solve.

        Args:
            data: Either a list of ``(arc_id, row_index, coeff)`` tuples or a
                  2-D numpy array with shape ``(N, 3)`` and columns
                  ``[arc_id, row_index, coeff]``.
        """
        if hasattr(data, "tolist"):
            self._rows_buffer.extend(data.tolist())
        else:
            self._rows_buffer.extend(data)

    def clone(self, include_rows: bool = True, clone_removed_arcs: bool = False) -> "ResourceGraph":
        """Return a deep clone of this ResourceGraph with stable arc IDs.

        Flushes all pending buffers before cloning so the clone reflects the
        complete current state.  The returned graph has an independent
        remove/restore state and its own resource factory copy.

        Args:
            include_rows: Copy arc rows into the clone (set *False* for a
                topology-only clone to be populated via :meth:`add_rows`).
            clone_removed_arcs: Also clone arcs currently removed from the
                graph (they will be re-removed in the clone).

        Returns:
            A new :class:`ResourceGraph` wrapping the cloned C++ object.
        """
        self._flush()
        cpp_clone = self._graph.clone(include_rows, clone_removed_arcs)
        cloned = ResourceGraph.__new__(ResourceGraph)
        cloned._pending = []
        cloned._refs = list(self._refs)
        cloned._graph = cpp_clone
        cloned._graph_canonical = self._graph_canonical
        cloned._registered_order = self._registered_order
        cloned._full_registration_order = list(self._full_registration_order)
        cloned._node_buffer = []
        cloned._arc_buffer = []
        cloned._rows_buffer = []
        cloned._reserve_hint = (0, 0)
        cloned._next_arc_id = cpp_clone.next_arc_id()
        return cloned

    def clone_topology(self) -> "ResourceGraph":
        """Clone topology only (arc rows are empty in the clone).

        Use :meth:`add_rows` or :meth:`add_rows_to_arc` to populate rows per
        (demand, time) slice after cloning.

        Returns:
            A new :class:`ResourceGraph` with no rows on arcs.
        """
        return self.clone(include_rows=False)

    def sort_nodes(self, comp=None):
        """Sort graph nodes in place.

        Args:
            comp: Optional callable ``(node1, node2) -> bool`` returning True when
                *node1* should come before *node2*.  When omitted, nodes are sorted
                by ascending ``node.id``.
        """
        self._flush()
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
                ``Algorithm.Pulling``, ``Algorithm.Greedy``, ``Algorithm.AStar``,
                or the equivalent strings ``'simple'``, ``'pushing'``, ``'pulling'``,
                ``'greedy'``, ``'astar'``.
            upper_bound: Prune paths with cost ≥ this value.
            params: :class:`AlgorithmParams` (defaults to ``AlgorithmParams()``).
            preprocess: Run preprocessing before solving.
            cost_index: Index within the cost resource type (the first ``real``
                or ``int`` slot in canonical order that the user registered).
                Defaults to 0.

        Returns:
            :class:`SolveResult` with a ``solutions`` list and an
            ``AlgorithmStatus`` indicating why the solver stopped.
        """
        if cost_index < 0:
            raise ValueError(f"cost_index must be non-negative, got {cost_index}")
        _ext.graph.check_interrupted()
        if params is None:
            params = _ext.graph.AlgorithmParams()
        self._flush()
        if isinstance(params, BucketAlgorithmParams):
            params = params._to_cpp(self._full_registration_order)
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
                                          for row in arc.rows)

        and writes ``reduced_cost`` to extender resource slot *cost_index*.
        ``arc.cost`` (base cost) is never modified, so repeated calls with
        different duals are correct across iterations.

        Args:
            duals: ``dict`` mapping row-index → dual value, **or** a ``list``
                   (or any sequence) where ``duals[i]`` is the dual for row *i*.
                   An empty dict is silently ignored.
            cost_index: Resource slot to update (default 0).
        """
        if cost_index < 0:
            raise ValueError(f"cost_index must be non-negative, got {cost_index}")
        self._flush()
        # The pybind binding only registers update_reduced_costs on graph
        # specialisations that include RealResource (see graph_impl.hpp). Without
        # this guard, int-only graphs raise a cryptic AttributeError referencing
        # the internal _core type name. Surface the real requirement instead.
        if not hasattr(self._graph, "update_reduced_costs"):  # pragma: no cover
            raise TypeError(
                "update_reduced_costs requires a graph with a RealResource cost "
                "slot; this graph has none."
            )

        if isinstance(duals, dict):
            if not duals:
                return  # honor the docstring: empty dict ⇒ leave reduced costs unchanged
            import numpy as np

            max_idx = max(duals.keys())
            duals_arr = np.zeros(max_idx + 1, dtype=np.float64)
            for k, v in duals.items():
                duals_arr[k] = v
        else:
            import numpy as np

            # ascontiguousarray is a no-op when duals is already a C-contiguous
            # float64 ndarray (the common hot-path case), avoiding tolist() overhead.
            duals_arr = np.ascontiguousarray(duals, dtype=np.float64)
        self._graph.update_reduced_costs(duals_arr, cost_index)

    # ── String representation ─────────────────────────────────────────────────

    def to_string(self, print_arcs: bool = True) -> str:
        """Return a human-readable string representation of the graph.

        Args:
            print_arcs: Include arc details in the output (default ``True``).

        Returns:
            Multi-line string describing the graph, or an empty string when the
            graph has not been initialised yet.
        """
        if self._graph is None and not self._node_buffer and not self._arc_buffer:
            return ""
        self._flush()
        return self._graph.to_string(print_arcs)

    def __str__(self) -> str:
        """Return a human-readable string representation of the graph."""
        return self.to_string()

    def __repr__(self) -> str:
        """Return a developer-friendly representation of the graph."""
        return self.to_string()

    # ── Transparent delegation for everything else ────────────────────────────

    def __getattr__(self, name: str):
        """Delegate unknown attribute lookups to the underlying C++ graph.

        Called only when normal attribute lookup fails.  Flushes pending
        buffers so the C++ object is up-to-date before the attribute is read.

        Args:
            name: Attribute name to look up on the C++ graph.

        Returns:
            The attribute value from the C++ graph object.
        """
        # Called only when normal lookup fails — flushes the buffer and forwards
        # to the C++ graph.
        self._flush()
        return getattr(self._graph, name)

    # ── NetworkX integration ──────────────────────────────────────────────────

    def from_networkx(self, nx_graph: nx.DiGraph):
        """Populate the graph from a NetworkX DiGraph.

        Nodes and arcs are buffered and flushed to C++ in a single batch.
        At least one node must be marked ``source=True`` and one ``sink=True``.
        When resources have already been registered, every edge must carry a
        ``'resource'`` attribute.

        Args:
            nx_graph: Directed NetworkX graph.  Each node may have ``source``
                and ``sink`` boolean attributes.  Each edge must have a
                ``'resource'`` attribute (tuple of consumption values) and may
                have a ``'cost'`` float and a ``'rows'`` list.

        Raises:
            ValueError: If no source or sink node is found, or if a required
                ``'resource'`` edge attribute is missing.
        """
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

        # ── Buffer nodes and arcs — flushed as one batch when graph is used ───
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
            cost = data.get("cost", 0.0)
            rows = data.get("rows", [])
            self.add_arc(resource_init, int(u), int(v), cost, rows)

        # Update the graph once everything is buffered
        self.update()


# ── Generate add_<type>_resource methods ─────────────────────────────────────


def _make_add_resource_method(canonical_type: str):
    def add_resource_method(
        self,
        extension_function,
        feasibility_function,
        cost_function,
        dominance_function,
    ):
        if self._graph is not None:
            raise RuntimeError(
                f"Cannot call add_{canonical_type}_resource after the graph has been "
                "initialized (i.e. after the first solve / update / get_node call). "
                "Add all resources before using the graph."
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

# ── BucketAlgorithmParams Python wrapper ─────────────────────────────────────
# Keep a reference to the raw C++ class so the wrapper can instantiate it.
_CppBucketAlgorithmParams = _ext.graph.BucketAlgorithmParams


class BucketAlgorithmParams:
    """AlgorithmParams variant using a bucket-partitioned label container.

    Pass ``bucket_resource_pos`` and/or ``sort_resource_pos`` to identify
    resources by their registration order (0 = first ``add_<type>_resource()``
    call, 1 = second, etc.).  Python resolves the position to the correct C++
    resource type and within-type index at solve time.

    When a position is not given (``None``), the corresponding C++ field
    (``bucket_resource_type`` / ``sort_resource_index``) is used as-is,
    preserving backward compatibility with code that sets those fields directly.

    Args:
        range_buckets: Width of each bucket along the bucket resource axis.
        bucket_resource_pos: Registration-order position of the resource used
            to partition labels into buckets.  ``None`` (default) keeps the
            C++ default (empty ``bucket_resource_type`` → cost resource).
        sort_resource_pos: Registration-order position of the resource used to
            sort labels within each bucket.  Must be a numerical resource
            (``real`` or ``int``).  ``None`` (default) keeps the C++ default
            (``sort_resource_index=0``).
        **kwargs: Additional :class:`AlgorithmParams` fields
            (e.g. ``stop_after_X_solutions``, ``timeout_s``, ``max_memory_gb``).
    """

    def __init__(
        self,
        range_buckets: int = 100,
        bucket_resource_pos=None,
        sort_resource_pos=None,
        **kwargs,
    ):
        """Create BucketAlgorithmParams with optional resource-position hints.

        Args:
            range_buckets: Width of each bucket partition along the bucket
                resource axis.
            bucket_resource_pos: Zero-based registration-order index of the
                resource used to partition labels into buckets.  ``None``
                keeps the C++ default.
            sort_resource_pos: Zero-based registration-order index of the
                numerical resource used to sort labels within each bucket.
                ``None`` keeps the C++ default.
            **kwargs: Additional :class:`AlgorithmParams` fields forwarded
                directly to the C++ object (e.g. ``stop_after_X_solutions``,
                ``timeout_s``, ``max_memory_gb``).
        """
        self._bucket_resource_pos = (
            None if bucket_resource_pos is None else int(bucket_resource_pos)
        )
        self._sort_resource_pos = None if sort_resource_pos is None else int(sort_resource_pos)
        self._cpp = _CppBucketAlgorithmParams()
        self._cpp.range_buckets = int(range_buckets)
        for k, v in kwargs.items():
            setattr(self._cpp, k, v)

    def __getattr__(self, name: str):
        """Forward public attribute reads to the underlying C++ params object.

        Args:
            name: Attribute name.

        Returns:
            The attribute value from the C++ ``BucketAlgorithmParams``.

        Raises:
            AttributeError: For private names (starting with ``_``).
        """
        if name.startswith("_"):
            raise AttributeError(name)
        return getattr(self._cpp, name)

    def __setattr__(self, name: str, value):
        """Forward public attribute writes to the underlying C++ params object.

        Private names (starting with ``_``) are stored on the Python instance
        via the default ``object.__setattr__`` mechanism.

        Args:
            name: Attribute name.
            value: Value to assign.
        """
        if name.startswith("_"):
            super().__setattr__(name, value)
        else:
            setattr(self._cpp, name, value)

    def _to_cpp(self, full_registration_order: list) -> "_CppBucketAlgorithmParams":
        """Resolve resource positions and return the underlying C++ params object.

        Args:
            full_registration_order: Per-instance list of canonical resource type
                names in the order ``add_<type>_resource()`` was called
                (from :attr:`ResourceGraph._full_registration_order`).

        Returns:
            The configured C++ ``BucketAlgorithmParams`` ready for ``solve()``.
        """
        reg = full_registration_order
        n = len(reg)

        def resolve(pos: int, label: str) -> tuple:
            if pos < 0 or pos >= n:
                raise ValueError(
                    f"{label}={pos} out of range; graph has {n} registered "
                    f"resource(s) (valid: 0–{n - 1})."
                )
            t = reg[pos]
            idx = sum(1 for r in reg[:pos] if r == t)
            return t, idx

        if self._bucket_resource_pos is not None:
            bucket_type, bucket_idx = resolve(self._bucket_resource_pos, "bucket_resource_pos")
            # Translate Python canonical name to the C++ prefix expected by run_bucket_solve.
            self._cpp.bucket_resource_type = CPP_NAME.get(bucket_type, bucket_type)
            self._cpp.bucket_resource_index = bucket_idx

        if self._sort_resource_pos is not None:
            _, sort_idx = resolve(self._sort_resource_pos, "sort_resource_pos")
            self._cpp.sort_resource_index = sort_idx

        return self._cpp
