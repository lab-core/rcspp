#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# Entry point for:  python -m vrp <instance> [options]
# Run from src/python/ so that the vrp package and rcspp are on the path.

import os
import sys

_src_python = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_python_interface = os.path.join(os.path.dirname(_src_python), "python_interface")

if _src_python not in sys.path:
    sys.path.insert(0, _src_python)
if _python_interface not in sys.path:
    sys.path.insert(0, _python_interface)

from vrp.vrp import main  # noqa: E402

main()
