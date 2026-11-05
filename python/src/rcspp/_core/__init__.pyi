"""
RCSPP module
"""

from __future__ import annotations

from . import graph, logger, resource, solution_pool

__all__: list[str] = [
    "available_memory_bytes",
    "graph",
    "logger",
    "process_memory_bytes",
    "resource",
    "solution_pool",
]

def available_memory_bytes() -> int:
    """
    Available system RAM in bytes.
    """

def process_memory_bytes() -> int:
    """
    Current process Resident Set Size (RSS) in bytes.
    """
