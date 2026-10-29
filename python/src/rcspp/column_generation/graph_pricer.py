#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The graph pricer: a :class:`~rcspp.graph.ResourceGraph` as a pricer the loop calls.

The pricer runs in C++: the loop calls it without Python, so pricing does not need the GIL.
"""

# The pricer reads the graph's buffers and resolves its stages as ResourceGraph.solve does.
# pyright: reportPrivateUsage=false

from dataclasses import dataclass
from typing import Any, Sequence

from .._core.column_generation import GraphPricer as _CppGraphPricer
from .._core.graph import Algorithm, AlgorithmParams, SearchDirection
from ..graph import BucketAlgorithmParams, ResourceGraph, _algorithm, _chosen_direction

# Algorithms that may miss a better path even when they finish.
_HEURISTICS = (Algorithm.Greedy, Algorithm.Tabu)


@dataclass(frozen=True)
class PricingStage:
    """One way of searching the pricing graph, with the arguments of
    :meth:`ResourceGraph.solve <rcspp.graph.ResourceGraph.solve>`.

    Args:
        algorithm: The algorithm, a name (``"simple"``, ``"greedy"``…) or an ``Algorithm``.
        params: Its :class:`AlgorithmParams` or :class:`BucketAlgorithmParams`; ``None``
            for the defaults. Copied when the pricer is built: changing them afterwards
            changes nothing. The time limit of a pricing call caps ``params.timeout_s``.
        direction: ``"forward"``, ``"backward"`` or ``"bidirectional"``; ``None`` as in
            ``solve``.
        exact: Whether a finished search proves that no better path exists. ``None``
            takes it from the algorithm: ``greedy`` and ``tabu`` are heuristics, the others
            exact. Only an exact stage that finds nothing lets the loop stop with a proof.
    """

    algorithm: str | Algorithm = "simple"
    params: AlgorithmParams | BucketAlgorithmParams | None = None
    direction: str | SearchDirection | None = None
    exact: bool | None = None


class GraphPricer(_CppGraphPricer):
    """A pricer over a :class:`~rcspp.graph.ResourceGraph` whose arcs carry master rows.

    Each call writes the reduced costs into the graph (an arc's cost minus the duals of its
    rows, in the cost component ``cost_index``), then runs the stages from the first: it
    stops at the first stage that finds a path below the threshold, or whose search proves
    that none exists. A call proves that no improving path exists only when an exact stage
    finished.

    The graph's nodes and arcs are sent to C++ when the pricer is built: add them before,
    or call ``graph.update()`` after adding more. The pricer keeps the graph alive; do not
    solve the graph elsewhere during a run.

    Args:
        graph: The pricing graph; its first resource is the real cost.
        stages: The stages, from the cheapest to the most exact; one exact ``simple``
            stage by default.
        cost_index: The cost component that holds the reduced costs.
        preprocess: Whether each search reduces the graph first.
    """

    def __init__(
        self,
        graph: ResourceGraph,
        stages: Sequence[PricingStage] | None = None,
        cost_index: int = 0,
        preprocess: bool = True,
    ) -> None:
        if cost_index < 0:
            raise ValueError(f"cost_index must be non-negative, got {cost_index}")
        graph._flush()
        resolved: list[tuple[Algorithm, Any, SearchDirection, bool]] = []
        for stage in stages if stages is not None else [PricingStage()]:
            algorithm = _algorithm(stage.algorithm)
            params: Any = stage.params if stage.params is not None else AlgorithmParams()
            if isinstance(params, BucketAlgorithmParams):
                params = params._to_cpp(graph._full_registration_order)
            direction = _chosen_direction(algorithm, params, stage.direction)
            exact = stage.exact if stage.exact is not None else algorithm not in _HEURISTICS
            resolved.append((algorithm, params, direction, exact))
        super().__init__(graph._graph, resolved, cost_index, preprocess)
        # The Python graph owns the resource functions its C++ graph points to.
        self._resource_graph = graph

    @property
    def graph(self) -> ResourceGraph:
        """The pricing graph."""
        return self._resource_graph
