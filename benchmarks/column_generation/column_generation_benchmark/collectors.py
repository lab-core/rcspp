#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What a run measures besides its totals, one collector per statistic.

A collector has a name, the key of the ``statistics`` block that turns it on, says what it
costs the run, and adds its fields to the run's ``result.json``.
"""

import sys
from collections.abc import Callable, Mapping
from dataclasses import dataclass
from typing import Any


@dataclass(frozen=True)
class Collector:
    """A statistic: the fields it adds, with their kinds, and how it measures them."""

    name: str
    cost: str
    fields: Mapping[str, str]
    measure: Callable[[], dict[str, Any]]


def measure_process() -> dict[str, Any]:
    """This process's peak memory and the processor time it used."""
    import psutil

    process = psutil.Process()
    memory = process.memory_info()
    peak = getattr(memory, "peak_wset", None)

    if peak is None:
        import resource

        # Kibibytes on Linux, bytes on macOS.
        maximum = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        peak = maximum if sys.platform == "darwin" else maximum * 1024

    times = process.cpu_times()
    return {
        "process_peak_memory_bytes": int(peak),
        "process_processor_seconds": times.user + times.system,
    }


#: Every collector, by the key of the ``statistics`` block that turns it on.
COLLECTORS: dict[str, Collector] = {
    "process": Collector(
        name="process",
        cost="two system calls once the run is over",
        fields={"process_peak_memory_bytes": "integer", "process_processor_seconds": "number"},
        measure=measure_process,
    ),
}


def collect(statistics: Mapping[str, Any]) -> dict[str, Any]:
    """The fields of every collector the run's ``statistics`` block turns on."""
    fields: dict[str, Any] = {}

    for name, collector in COLLECTORS.items():
        if statistics.get(name):
            fields.update(collector.measure())

    return fields
