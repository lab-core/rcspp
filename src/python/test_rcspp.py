#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa
from unicodedata import name

from vrp.instance_reader import InstanceReader
from utils.definitions import INSTANCES_DIR
from utils.test_utils import *

if __name__ == "__main__":
    solutions = {}
    print("Read instance...")
    instance_name = "R101"
    instance_path = INSTANCES_DIR + instance_name + ".txt"
    instance_reader = InstanceReader(instance_path)
    instance = instance_reader.read()

    _, _, sol_dict = vrp_instance(instance, dir="test", verbose=False)

    print(sol_dict)