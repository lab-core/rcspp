#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The graph pricer: a :class:`~rcspp.graph.ResourceGraph` as a pricer the loop calls."""

# The pricer reads the graph's buffers and resolves its stages as ResourceGraph.solve does.
# pyright: reportPrivateUsage=false

import math
import operator
import time
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from typing import Any, TypeAlias, cast

from .._core.graph import Algorithm, AlgorithmParams, AlgorithmStatus, SearchDirection, SolveResult
from ..graph import BucketAlgorithmParams, ResourceGraph, _algorithm, _chosen_direction
from .pricer import Pricer, PricingRequest, PricingResult, PricingStatus
from .types import FloatArray

# Algorithms that may miss a better path even when they finish.
_HEURISTICS = (Algorithm.Greedy, Algorithm.Tabu)


@dataclass(frozen=True, kw_only=True)
class StageRequest:
    """What a pricing stage is asked to search."""

    #: Keep only the paths whose cost (the reduced cost) is below this.
    upper_bound: float = 0.0
    #: Wall-clock limit in seconds; caps the stage's own limit.
    timeout_s: float = math.inf
    #: Whether the graph is reduced before the search.
    preprocess: bool = True
    #: The cost component that holds the reduced costs.
    cost_index: int = 0


#: A stage's own search: ``search(graph, request)`` searches ``graph`` as ``request`` asks.
Search: TypeAlias = Callable[[ResourceGraph, StageRequest], SolveResult]


@dataclass(frozen=True)
class PricingStage:
    """One way of searching the pricing graph: an algorithm, with the arguments of
    :meth:`ResourceGraph.solve <rcspp.graph.ResourceGraph.solve>`, or the user's own search.

    Args:
        algorithm: The algorithm, a name (``"simple"``, ``"greedy"``...) or an ``Algorithm``.
        params: Its :class:`AlgorithmParams` or :class:`BucketAlgorithmParams`; ``None``
            for the defaults. Copied when the pricer is built: changing them afterwards
            changes nothing. The time limit of a pricing call caps ``params.timeout_s``.
        direction: ``"forward"``, ``"backward"`` or ``"bidirectional"``; ``None`` as in
            ``solve``.
        exact: Whether a finished search proves that no better path exists. ``None``
            takes it from the algorithm: ``greedy`` and ``tabu`` are heuristics, the others
            exact. Only an exact stage that finds nothing lets the loop stop with a proof.
        search: The user's own search, run instead of an algorithm, e.g. several calls of
            ``solve``: ``search(graph, request)`` returns a :class:`SolveResult` and must
            honor every field of the :class:`StageRequest`. It needs ``exact``, and takes no
            other ``algorithm`` than the default, no ``params`` and no ``direction``.

    Raises:
        TypeError: When ``search`` is not callable.
        ValueError: When ``search`` is given without ``exact``, or with an algorithm,
            ``params`` or ``direction``.
    """

    algorithm: str | Algorithm = "simple"
    params: AlgorithmParams | BucketAlgorithmParams | None = None
    direction: str | SearchDirection | None = None
    exact: bool | None = None
    search: Search | None = None

    def __post_init__(self) -> None:
        if self.search is None:
            return

        if not callable(self.search):
            raise TypeError("PricingStage: the search must be callable")

        if self.exact is None:
            raise ValueError("PricingStage: a stage with its own search must say if it is exact")

        if self.algorithm != "simple" or self.params is not None or self.direction is not None:
            raise ValueError(
                "PricingStage: a stage with its own search takes no algorithm, params or direction"
            )


@dataclass(frozen=True, eq=False)
class _ResolvedStage:
    search: Search
    exact: bool


def _copied(params: Any) -> Any:
    """A copy of search parameters: their classes have no copy of their own.

    Only the attributes that can be set are copied; a read-only property is computed.
    """
    kind = params.__class__
    copy = kind()

    for name in dir(params):
        if name.startswith("_") or callable(getattr(params, name)):
            continue

        attribute = getattr(kind, name, None)

        if isinstance(attribute, property) and attribute.fset is None:
            continue

        setattr(copy, name, getattr(params, name))

    return copy


def _algorithm_search(algorithm: Algorithm, params: Any, direction: SearchDirection) -> Search:
    """The search of an algorithm stage: ``solve`` with ``params``, its time limit capped by
    the request's."""
    timeout_s = params.timeout_s

    def search(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        params.timeout_s = min(timeout_s, request.timeout_s)
        return graph.solve(
            algorithm,
            request.upper_bound,
            params,
            request.preprocess,
            request.cost_index,
            direction,
        )

    return search


def _checked_search(search: Search) -> Search:
    """The user's search, its result checked: anything else would fail deep inside."""

    def checked(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        searched: object = search(graph, request)

        if not isinstance(searched, SolveResult):  # pyright: ignore[reportUnnecessaryIsInstance]
            raise TypeError(
                f"PricingStage.search: expected a SolveResult, got {type(searched).__name__}"
            )

        return searched

    return checked


class GraphPricer(Pricer):
    """A pricer over a :class:`~rcspp.graph.ResourceGraph` whose arcs carry master rows.

    Each call writes the reduced costs into the graph (an arc's cost minus the duals of its
    rows, in the cost component ``cost_index``), then runs the stages from the first: it
    stops at the first stage that finds a path below the threshold, or whose search proves
    that none exists. A call proves that no improving path exists only when an exact stage
    finished. :attr:`last_stage_index` tells which stage the last call ended with.

    The pricer keeps the graph alive; do not solve the graph elsewhere during a run.

    Args:
        graph: The pricing graph; its first resource is the real cost.
        stages: The stages, from the cheapest to the most exact; one exact ``simple``
            stage by default.
        cost_index: The cost component that holds the reduced costs.
        preprocess: Whether each search reduces the graph first.

    Raises:
        TypeError: When ``graph`` is not a :class:`~rcspp.graph.ResourceGraph`, a stage is not a
            :class:`PricingStage`, or ``cost_index`` is not an integer.
        ValueError: For a negative ``cost_index``, no stage, or a stage whose algorithm or
            direction the graph refuses.
    """

    def __init__(
        self,
        graph: ResourceGraph,
        stages: Sequence[PricingStage] | None = None,
        cost_index: int = 0,
        preprocess: bool = True,
    ) -> None:
        if not isinstance(graph, ResourceGraph):  # pyright: ignore[reportUnnecessaryIsInstance]
            raise TypeError("GraphPricer: expected a ResourceGraph")

        try:
            cost_index = operator.index(cost_index)
        except TypeError:
            raise TypeError("GraphPricer: cost_index must be an integer") from None

        if cost_index < 0:
            raise ValueError(f"cost_index must be non-negative, got {cost_index}")

        given: list[object] = list(stages) if stages is not None else [PricingStage()]

        if not given:
            raise ValueError("GraphPricer: no stage")

        if not all(isinstance(stage, PricingStage) for stage in given):
            raise TypeError("GraphPricer: stages must be PricingStage objects")

        # The graph's internals are not annotated.
        self._internals = cast(Any, graph)

        # The registered resources are known once the graph is built.
        self._internals._flush()
        self._graph = graph
        self._cost_index = cost_index
        self._preprocess = preprocess
        self._stages = tuple(cast(list[PricingStage], given))
        self._resolved_stages = [self._resolved(stage) for stage in self._stages]
        self._last_stage_index: int | None = None

    @property
    def graph(self) -> ResourceGraph:
        """The pricing graph."""
        return self._graph

    @property
    def stages(self) -> tuple[PricingStage, ...]:
        """The stages, from the cheapest to the most exact."""
        return self._stages

    @property
    def last_stage_index(self) -> int | None:
        """The position of the stage the last call ended with; ``None`` before the first
        call."""
        return self._last_stage_index

    def price(self, duals: FloatArray, request: PricingRequest = PricingRequest()) -> PricingResult:
        """Prices the duals: one per master row.

        Args:
            duals: The duals.
            request: The threshold and the time limit.

        Returns:
            The paths below the threshold as columns, each with its original cost and
            rows. The status is ``Optimal`` and ``min_reduced_cost_bound`` the best path
            found (the threshold when there is none) only after an exact stage
            finished; otherwise ``SolverLimit`` and ``-inf``.

        Raises:
            RuntimeError: When the graph has no source or no sink.
            IndexError: When the cost index is not a cost component of the graph.
        """
        graph = self._graph

        # A search that cannot start returns nothing, which would read as a proof.
        if not graph.source_node_ids() or not graph.sink_node_ids():
            raise RuntimeError("GraphPricer.price: the graph has no source or no sink")

        # The engine would only warn, and search the costs left from before.
        if self._cost_index >= self._internals._full_registration_order.count("real"):
            raise IndexError(
                "GraphPricer.price: the cost index is not a cost component of the graph"
            )

        graph.update_reduced_costs(duals, self._cost_index)

        start = time.perf_counter()
        *first, last = self._resolved_stages

        for index, stage in enumerate(first):
            self._last_stage_index = index
            result = self._search(stage, request, start)

            # Only an exact stage that finished is optimal.
            proven = result.status == PricingStatus.Optimal
            out_of_time = time.perf_counter() - start >= request.timeout_s

            if result.columns or proven or out_of_time:
                return result

        self._last_stage_index = len(first)
        return self._search(last, request, start)

    def cost_scale(self) -> float:
        """The largest absolute arc cost (original costs, not reduced ones)."""
        self._internals._flush()
        graph = self._internals._graph
        return max((abs(graph.get_arc(arc_id).cost) for arc_id in graph.arc_ids()), default=0.0)

    def _search(
        self, stage: _ResolvedStage, request: PricingRequest, start: float
    ) -> PricingResult:
        """Runs one stage within what is left of the request's time."""
        stage_request = StageRequest(
            upper_bound=request.reduced_cost_threshold,
            timeout_s=max(0.0, request.timeout_s - (time.perf_counter() - start)),
            preprocess=self._preprocess,
            cost_index=self._cost_index,
        )
        searched = stage.search(self._graph, stage_request)
        return _to_pricing_result(searched, stage.exact, request.reduced_cost_threshold)

    def _resolved(self, stage: PricingStage) -> _ResolvedStage:
        """The stage as a search: its own, or its algorithm's (as ``ResourceGraph.solve``
        resolves it)."""
        if stage.search is not None:
            return _ResolvedStage(_checked_search(stage.search), bool(stage.exact))

        algorithm = _algorithm(stage.algorithm)
        params = stage.params if stage.params is not None else AlgorithmParams()

        if isinstance(params, BucketAlgorithmParams):
            params = params._to_cpp(self._internals._full_registration_order)

        params = _copied(params)
        direction = _chosen_direction(algorithm, params, stage.direction)
        exact = stage.exact if stage.exact is not None else algorithm not in _HEURISTICS
        return _ResolvedStage(_algorithm_search(algorithm, params, direction), exact)


def _to_pricing_result(searched: SolveResult, exact: bool, threshold: float) -> PricingResult:
    """Keeps the paths below the threshold, and proves only when an exact search
    finished (a complete search that memory pressure did not trim)."""
    result = PricingResult(exact_stage=exact, status=PricingStatus.SolverLimit)

    best = threshold

    for solution in searched.solutions:
        best = min(best, solution.cost)

        if solution.cost < threshold:
            result.columns.append(solution)

    finished = (
        searched.status == AlgorithmStatus.Complete and not searched.memory_pressure_triggered
    )

    if exact and finished:
        result.status = PricingStatus.Optimal
        result.min_reduced_cost_bound = best

    return result
