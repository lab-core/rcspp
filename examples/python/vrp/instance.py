#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""A vehicle routing problem with time windows, read from a file in Solomon's format."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path

# The repository's instances, used when a path given is not found.
INSTANCES_DIRECTORY = Path(__file__).resolve().parents[3] / "instances"


@dataclass(frozen=True)
class Customer:
    """A customer, or the depot (number 0): where it is, what it asks for and when."""

    number: int
    x: float
    y: float
    demand: float
    ready_time: float
    due_time: float
    service_time: float


@dataclass(frozen=True)
class Instance:
    """The vehicles and the customers, the depot first."""

    name: str
    vehicles: int
    capacity: float
    customers: list[Customer]

    @property
    def depot(self) -> Customer:
        return self.customers[0]


def distance(first: Customer, second: Customer) -> float:
    """The Euclidean distance between two customers, also the travel time."""
    return math.hypot(second.x - first.x, second.y - first.y)


def find_instance(path: str) -> Path:
    """The file ``path``, or a file of that name in the repository's ``instances/``.

    ``C101_25``, ``C101_25.txt`` and ``instances/C101_25.txt`` all find the same file.
    """
    given = Path(path)

    if given.exists():
        return given

    name = given.name if given.suffix else given.name + ".txt"
    candidate = INSTANCES_DIRECTORY / name

    if candidate.exists():
        return candidate

    raise FileNotFoundError(f"no instance {path!r}, nor {name!r} in {INSTANCES_DIRECTORY}")


def read_solomon(path: str | Path) -> Instance:
    """Reads a Solomon file: a title line, the line with the number of vehicles and their
    capacity, then one line of seven numbers per customer (number, x, y, demand, ready time,
    due time, service time), the depot first. Other lines are headers.
    """
    path = Path(path)
    lines = path.read_text().splitlines()
    vehicles: tuple[int, float] | None = None
    customers: list[Customer] = []

    for line in lines[1:]:
        try:
            numbers = [float(field) for field in line.split()]
        except ValueError:
            continue

        if len(numbers) == 2 and vehicles is None:
            vehicles = (int(numbers[0]), numbers[1])
        elif len(numbers) == 7:
            customers.append(Customer(int(numbers[0]), *numbers[1:]))

    if vehicles is None or not customers:
        raise ValueError(f"{path}: no vehicle line or no customers")

    if [customer.number for customer in customers] != list(range(len(customers))):
        raise ValueError(f"{path}: the customers must be numbered 0, 1, 2, ..., the depot 0")

    # The name line gives the full instance's name (C101 in C101_25.txt): the file's is clearer.
    return Instance(path.stem, vehicles[0], vehicles[1], customers)
