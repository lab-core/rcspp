#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa
from unicodedata import name

from utils.import_rscpp_lib import import_rscpp_lib
import_rscpp_lib()

from vrp.instance_reader import InstanceReader

from utils.test_utils import *

if __name__ == "__main__":
    solutions = {}
    print("Read instance...")
    instance_name = "R101"
    instance_path = "../../instances/" + instance_name + ".txt"
    instance_reader = InstanceReader(instance_path)
    instance = instance_reader.read()
    dual_optimal_solution = instance_reader.read_dual_optimal(instance.get_name())

    alphas = [1/10 for i in range(1, 2)]

    for alpha in alphas:
        vrp, solution, sol_dict = vrp_instance(instance, smoothing=alpha, smoothing_center=dual_optimal_solution)

        solutions[instance_name+f"_{alpha}"] = sol_dict
        
    
    print(solutions)
    save_dict_to_json("Smoothing", "summary", solutions)