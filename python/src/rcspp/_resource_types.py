#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Resource type constants and utilities for the Python RCSPP API.

Defines the set of resource types exposed to Python users and helpers for mapping them
to the underlying C++ template naming conventions.
"""

# ── Python-meaningful resource types ─────────────────────────────────────────
# uint / size_t variants are C++ implementation details with no semantic difference
# in Python (Python integers are arbitrary-precision and unsigned/signed distinctions
# don't apply).  Only the types below are exposed in the Python API.
NUMERICAL = ["real", "int"]
CONTAINER = ["real_set", "int_set", "bitset"]
ALL = NUMERICAL + CONTAINER

# ── C++ name translation ──────────────────────────────────────────────────────
# Maps a Python type name to the C++ prefix used in method/class names when they differ.
CPP_NAME: dict[str, str] = {
    "bitset": "uint_bitset",  # Python "bitset" → C++ UIntBitsetResource
}

# ── Canonical ordering ────────────────────────────────────────────────────────
# Matches the C++ ResourceGraph<...> template-parameter order.
_ORDER: dict[str, int] = {name: i for i, name in enumerate(ALL)}


def canonical(*types: str) -> tuple[str, ...]:
    """Return types sorted into canonical C++ template-slot order.

    The order matches the ResourceGraph<...> template-parameter sequence used
    in the C++ library, ensuring consistent type-key construction.

    Args:
        *types: One or more resource type names (e.g. ``"real"``,
            ``"int_set"``, ``"bitset"``).

    Returns:
        A tuple of the supplied type names in canonical order.
    """
    return tuple(sorted(types, key=lambda t: _ORDER[t]))
