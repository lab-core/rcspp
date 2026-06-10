#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# Entry point for:  python -m vrp <instance> [options]
# The vrp example package lives under examples/python/; the rcspp package it
# depends on lives under python/src/. Add both to the path.

import os
import sys

_examples_python = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_repo_root = os.path.dirname(os.path.dirname(_examples_python))
_rcspp_src = os.path.join(_repo_root, "python", "src")

for _p in (_examples_python, _rcspp_src):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from vrp.vrp import main  # noqa: E402

main()
