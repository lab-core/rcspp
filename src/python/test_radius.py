#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

from vrp.instance_reader import InstanceReader
from utils.test_utils import *
from utils.definitions import INSTANCES_DIR



if __name__ == "__main__":
    
    print("Read instance...")
    instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]
    #instances_name = ["R101"]

    solutions = {}
    alphas = [i/10 for i in range(1, 10)]

    for name in instances_name:
        instance_path = INSTANCES_DIR + name + ".txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        dual_optimal_solution = instance_reader.read_dual_optimal(instance.get_name())
        for alpha in alphas:
            _, _, sol_dict = vrp_instance(instance, smoothing=alpha, smoothing_center=dual_optimal_solution, verbose=False)
            solutions[name+f"_{alpha}"] = sol_dict

    save_dict_to_json("Smoothing", "Study_for_each_instance", solutions)