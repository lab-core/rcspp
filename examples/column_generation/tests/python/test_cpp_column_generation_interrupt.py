#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Ctrl-C during the example's pricing: a SIGINT stops the graph pricer, alone and in the
loop, and reaches the caller as KeyboardInterrupt, as it stops rcspp's own solve.

The example adds no signal code: its bindings run in rcspp's interruptible calls, and its
pricer's searches poll rcspp's interrupt flag. An interrupted search ends its stage without a
proof, the remaining stages stop at once, and the loop ends; the binding then raises.

POSIX only, as rcspp's own test: the signal is sent to the thread that prices
(signal.pthread_kill), so that it lands while the C++ search runs.
"""

import os
import signal
import sys
import threading
import time
from typing import Callable

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# First: importing the fakes puts the example's package on the path.
from cpp_column_generation_fakes import ScriptedMasterSolver  # noqa: E402

# isort: split
from cpp_column_generation import (  # noqa: E402
    ColumnGeneration,
    GraphPricer,
    MasterProblem,
    PricingRequest,
    RowSense,
    SolverStatus,
)

from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    SupersetDominanceFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    UnionExtensionFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

pytestmark = pytest.mark.skipif(sys.platform == "win32", reason="signal.pthread_kill is POSIX only")

# Nodes of the long graph: 2^(NODES - 2) paths, all kept (see long_graph). Seconds of search at
# 14, so the signal, sent after SIGNAL_DELAY_S, lands well inside it.
NODES = 14
SIGNAL_DELAY_S = 0.05
# The longest wait for the interrupted call to return: far less than a full search.
RETURN_TIMEOUT_S = 3.0


def long_graph() -> ResourceGraph:
    """A graph whose search keeps every partial path: each arc adds a bit of its own to a
    bitset resource whose dominance needs a superset, so no label dominates another. The
    arc costs are negative, so that every path is below the pricing threshold and none is
    cut by the bound either. Each arc out of the source covers master row 0."""
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    graph.add_bitset_resource(
        UnionExtensionFunction(),
        TrivialFeasibilityFunction(),
        TrivialCostFunction(),
        SupersetDominanceFunction(),
    )
    for node in range(NODES):
        graph.add_node(node, source=node == 0, sink=node == NODES - 1)
    arc_id = 0
    for tail in range(NODES):
        for head in range(tail + 1, NODES):
            cost = -float(head - tail)
            rows = [(0, 1.0)] if tail == 0 else []
            graph.add_arc((cost, {arc_id}), tail, head, cost=cost, rows=rows)
            arc_id += 1
    return graph


def interrupted(call: Callable[[], object]) -> bool:
    """Runs `call` in a thread, sends that thread SIGINT once the call has started, and
    returns whether the call raised KeyboardInterrupt. Fails if it has not returned
    RETURN_TIMEOUT_S after the signal."""
    raised = False
    thread_id: list[int] = []
    started = threading.Event()

    def run() -> None:
        nonlocal raised
        thread_id.append(threading.get_ident())
        started.set()
        try:
            call()
        except KeyboardInterrupt:
            raised = True

    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    started.wait()
    time.sleep(SIGNAL_DELAY_S)
    assert sys.platform != "win32"  # skipped there; tells the type checker too
    signal.pthread_kill(thread_id[0], signal.SIGINT)
    thread.join(timeout=RETURN_TIMEOUT_S)
    assert not thread.is_alive(), "the interrupted call did not return"
    return raised


def test_sigint_stops_the_graph_pricer() -> None:
    pricer = GraphPricer(long_graph())

    raised = interrupted(
        lambda: pricer.price(np.array([0.0]), PricingRequest(reduced_cost_threshold=0.0))
    )

    assert raised, "Expected KeyboardInterrupt from SIGINT during price()"


def test_sigint_stops_the_loop() -> None:
    # One row, "cover row 0 >= 1", which the artificial column covers first; the LP answers
    # with its cost as the dual, so that pricing searches the whole long graph.
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        lp.dual_values = [lp.columns[0].cost]
        lp.objective_value = lp.columns[0].cost
        lp.values = {0: 1.0}
        return SolverStatus.Optimal

    solver.on_solve = solve
    master.add_pricer(GraphPricer(long_graph()))

    raised = interrupted(lambda: ColumnGeneration(master).run())

    assert raised, "Expected KeyboardInterrupt from SIGINT during run()"
