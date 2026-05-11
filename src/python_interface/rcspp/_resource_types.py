#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

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
    """Return *types* sorted into canonical C++ template-slot order."""
    return tuple(sorted(types, key=lambda t: _ORDER[t]))


# ── Backward-compatibility aliases ────────────────────────────────────────────
# Maps old/C++-specific user-facing names → canonical Python type names.
# add_<alias>_resource() still works but internally uses the canonical type.
ALIASES: dict[str, str] = {
    "uint": "int",
    "uint_set": "int_set",
    "size_t_set": "int_set",
    "uint_bitset": "bitset",
    "size_t_bitset": "bitset",
    # "float":   "real",    # uncomment to accept "float" as alias for "real"
    # "integer": "int",
}
