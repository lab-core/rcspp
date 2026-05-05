#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from . import _core as _ext

# Directly re-export everything from resource submodule
globals().update(
    {k: getattr(_ext.resource, k) for k in dir(_ext.resource) if not k.startswith("_")}
)
