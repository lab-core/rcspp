#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import os
import sys

relative_path = "../../out/build/x64-release/lib/"
absolute_path = os.path.abspath(relative_path)
os.add_dll_directory(absolute_path)
sys.path.append(absolute_path)

rcspp_path = relative_path
sys.path.append(rcspp_path)

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
