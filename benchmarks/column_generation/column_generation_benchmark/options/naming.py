#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A run's name, the name of its folders: read from its sources once it is expanded.

A name is the labels of :data:`NAMED_OPTIONS`, then of every other block that differs from
the defaults. A block's label is the name it was given by (``one-thread``), or its type, or
a fingerprint of its table; then each dotted name that changed it (``cost_factor-1000.0``).
An instance's label is its name alone.
"""

import hashlib
import json
import re
from collections.abc import Mapping, Sequence
from typing import Any, Protocol, cast

from .schema import RUN_OPTIONS

#: The options whose labels every run's name has, in that order.
NAMED_OPTIONS = ("instance", "layer", "master", "pricing", "start")


class Source(Protocol):
    @property
    def options(self) -> Mapping[str, Any]:
        """The options the source gives."""
        ...


def label_part(value: Any) -> str:
    """`value` as a piece of a folder name."""
    text = value if isinstance(value, str) else json.dumps(value, sort_keys=True)
    return re.sub(r"[^A-Za-z0-9._]+", "_", text).strip("_")


def digest(value: Any) -> str:
    """A short fingerprint of a value."""
    text = json.dumps(value, sort_keys=True, default=str)
    return hashlib.sha256(text.encode()).hexdigest()[:6]


def labels(sources: Sequence[Source]) -> dict[str, list[str]]:
    """Per option, the name it was last given by (empty for a table), then the dotted
    names given after it."""
    found: dict[str, list[str]] = {}

    for source in sources:
        dotted: list[tuple[str, str]] = []

        for name, value in source.options.items():
            option, _, rest = name.partition(".")

            if rest:
                dotted.append((option, f"{rest.split('.')[-1]}-{label_part(value)}"))
            elif isinstance(value, str):
                found[option] = [value]
            elif isinstance(value, dict) and list(cast(dict[str, Any], value)) == ["type"]:
                found[option] = [cast(dict[str, Any], value)["type"]]
            else:
                found[option] = [""]

        for option, part in dotted:
            found.setdefault(option, [""]).append(part)

    return found


def label(option: str, value: Any, given: list[str]) -> str:
    if option == "instance" and "name" in value:
        return label_part(value["name"])

    if option == "layer":
        return value

    base, *dotted = given or [""]
    base = base or f"{value.get('type') or option}_{digest(value)}"
    return "-".join([base, *dotted])


def run_name(
    sources: Sequence[Source], options: Mapping[str, Any], defaults: Mapping[str, Any]
) -> str:
    """The name of a run given by `sources` and expanded to `options`."""
    given = labels(sources)
    parts = [label(option, options[option], given.get(option, [])) for option in NAMED_OPTIONS]

    for option in RUN_OPTIONS:
        if option not in NAMED_OPTIONS and options[option] != defaults.get(option):
            parts.append(label(option, options[option], given.get(option, [])))

    return "-".join(parts)


def unique_names(runs: Sequence[Any]) -> list[tuple[str, dict[str, Any]]]:
    """The runs as (name, options), a fingerprint of their options added to every name
    more than one has."""
    counts: dict[str, int] = {}

    for run in runs:
        counts[run.name] = counts.get(run.name, 0) + 1

    return [
        (f"{run.name}-{digest(run.options)}" if counts[run.name] > 1 else run.name, run.options)
        for run in runs
    ]


def folder_name(run: Any, round_number: int) -> str:
    """The folder of a run's round: ``<name>-r<round>``."""
    return f"{run.name}-r{round_number}"
