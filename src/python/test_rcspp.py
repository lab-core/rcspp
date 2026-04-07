#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import os
import sys

os_name = sys.platform

if os_name == 'win32':
    relative_path = "../../out/build/x64-release/lib/"
    absolute_path = os.path.abspath(relative_path)
    os.add_dll_directory(absolute_path)
    sys.path.append(absolute_path)

    rcspp_path = relative_path + "/rcspp/"
    sys.path.append(rcspp_path)
elif os_name == 'darwin':
    relative_path = "../../out/build/x64-release/lib/"
    absolute_path = os.path.abspath(relative_path)
    os.add_dll_directory(absolute_path)
    sys.path.append(absolute_path)

    rcspp_path = relative_path + "/rcspp/"
    sys.path.append(rcspp_path)
elif os_name == 'linux':
    relative_path = "../../build/lib/"
    absolute_path = os.path.abspath(relative_path)
    if "LD_LIBRARY_PATH" in os.environ:
        os.environ["LD_LIBRARY_PATH"] = f"{absolute_path}:{os.environ['LD_LIBRARY_PATH']}"
    else:
        os.environ["LD_LIBRARY_PATH"] = absolute_path
    sys.path.append(absolute_path)

    rcspp_path = relative_path
    sys.path.append(rcspp_path)
else:
    print(f"OS inconnu: {os_name}")

from vrp.instance_reader import InstanceReader

from vrp.vrp import VRP

if __name__ == "__main__":
    print("Read instance...")
    instance_name = "R101"
    instance_path = "../../instances/" + instance_name + ".txt"
    instance_reader = InstanceReader(instance_path)
    instance = instance_reader.read()

    print("Construct VRP")
    vrp = VRP(instance)
    print("Construct VRP ...Done")

    vrp.solve()
