#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The machine's load while a run goes, which the benchmark samples from outside the
run.

A sample holds the processors' busy and total time, the memory in use, the processors'
frequency and whether the machine is on battery. The benchmark takes one when a run starts,
one every :data:`INTERVAL_SECONDS` while it goes and one when it ends, with the run's own
processor time at each, so that a run shorter than the interval still has a start and an end.
What the other processes used is the processors' busy time less the run's.

The run's own processor time is read at every look the benchmark takes at its runs, and
once more when it has ended. Windows still gives it then; Linux does not, so there the run's
last moments before it ends (at most one look, 50 ms) count as the other processes'.
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    import psutil

#: The seconds between two samples while a run goes.
INTERVAL_SECONDS = 1.0


@dataclass(frozen=True)
class Sample:
    """The machine at one time: processor times are summed over every logical processor."""

    time: float
    busy_seconds: float
    total_seconds: float
    memory_used_bytes: int
    frequency_mhz: float | None
    on_battery: bool | None


def take_sample() -> Sample:
    """The machine now."""
    import psutil

    times = psutil.cpu_times()
    total = sum(times)
    # As psutil counts it: what is not idle, nor waiting for a disk.
    busy = total - times.idle - getattr(times, "iowait", 0.0)
    memory = psutil.virtual_memory()
    return Sample(
        time=time.monotonic(),
        busy_seconds=busy,
        total_seconds=total,
        memory_used_bytes=memory.total - memory.available,
        frequency_mhz=frequency(),
        on_battery=on_battery(),
    )


def frequency() -> float | None:
    """The processors' current frequency, in MHz; None when the system does not give it.

    Windows often gives the nominal frequency, whatever the current one.
    """
    import psutil

    try:
        current = psutil.cpu_freq()
    except (AttributeError, NotImplementedError, OSError):
        return None

    # Some systems give None, whatever psutil's types say.
    value = getattr(current, "current", None)
    return None if value is None else float(value)


def on_battery() -> bool | None:
    """Whether the machine runs on its battery; None without a battery, or when the
    system does not say."""
    import psutil

    sensors = getattr(psutil, "sensors_battery", None)

    try:
        battery = sensors() if sensors is not None else None
    except (NotImplementedError, OSError):
        return None

    if battery is None or battery.power_plugged is None:
        return None

    return not battery.power_plugged


def tree_processor_seconds(process: psutil.Process) -> float | None:
    """The processor time of `process` and of the children it still has; None once it is
    gone."""
    import psutil

    try:
        tree = [process, *process.children(recursive=True)]
    except psutil.Error:
        return None

    seconds = 0.0

    for member in tree:
        try:
            times = member.cpu_times()
        except psutil.Error:
            # A child that just ended takes its time with it; the run's own process does not.
            if member is process:
                return None

            continue

        seconds += times.user + times.system

    return seconds


@dataclass
class Load:
    """The samples of one run, each with the run's processor time then."""

    samples: list[tuple[Sample, float]] = field(default_factory=list[tuple[Sample, float]])

    def add(self, sample: Sample, run_seconds: float) -> None:
        self.samples.append((sample, run_seconds))

    def summary(self) -> dict[str, Any]:
        """The fields of the run's ``load``: means over the whole run, maxima and minima
        over its intervals or samples; null for what the system did not give.

        The processor's figures need two samples, which every run the benchmark ends
        has: its start and its end.
        """
        others = [
            other_percent(earlier, later)
            for earlier, later in zip(self.samples, self.samples[1:], strict=False)
        ]
        memory = [sample.memory_used_bytes for sample, _ in self.samples]
        frequencies = [s.frequency_mhz for s, _ in self.samples if s.frequency_mhz is not None]
        batteries = [s.on_battery for s, _ in self.samples if s.on_battery is not None]
        return {
            "interval_seconds": INTERVAL_SECONDS,
            "samples": len(self.samples),
            "other_processor_percent_mean": (
                other_percent(self.samples[0], self.samples[-1]) if others else None
            ),
            "other_processor_percent_max": max(others, default=None),
            "memory_used_bytes_mean": round(sum(memory) / len(memory)) if memory else None,
            "memory_used_bytes_max": max(memory, default=None),
            "frequency_mhz_mean": (sum(frequencies) / len(frequencies) if frequencies else None),
            "frequency_mhz_min": min(frequencies, default=None),
            "on_battery": any(batteries) if batteries else None,
        }


def other_percent(earlier: tuple[Sample, float], later: tuple[Sample, float]) -> float:
    """What the other processes used of every processor between two samples, in
    percent."""
    (start, run_start), (end, run_end) = earlier, later
    total = end.total_seconds - start.total_seconds

    if total <= 0.0:
        return 0.0

    busy = end.busy_seconds - start.busy_seconds - (run_end - run_start)
    return 100.0 * min(max(busy, 0.0), total) / total
