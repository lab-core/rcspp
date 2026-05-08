#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import utils.rscpp_lib
from vrp.instance_reader import InstanceReader
from utils.definitions import *
from vrp.stabilized_vrp import StabilizedVRP


if __name__ == "__main__":
    
    print("Read instance...")
    instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]
    instances_name = ["R101"]
    verbose = True

    for name in instances_name:
        instance_path = f"{INSTANCES_DIR}{name}.txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        vrp = StabilizedVRP(instance, verbose=verbose)
        vrp.solve()