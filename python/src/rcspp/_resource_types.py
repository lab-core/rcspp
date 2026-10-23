#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Resource type constants and utilities for the Python RCSPP API.

Defines the set of resource types exposed to Python users and helpers for mapping them
to the underlying C++ template naming conventions.
"""

# ── Python-meaningful resource types ─────────────────────────────────────────
# The bindings register exactly these names (python/bindings/resource_types.hpp): "bitset" is
# the C++ UIntBitsetResource.
NUMERICAL = ["real", "int"]
CONTAINER = ["real_set", "int_set", "bitset"]
ALL = NUMERICAL + CONTAINER

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
