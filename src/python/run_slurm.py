#!/usr/bin/env python3

import os
import sys
from utils.test_utils import *
from vrp.instance_reader import InstanceReader

dir = "all_solutions"
verbose = True

try:
    task_id = int(os.environ['SLURM_ARRAY_TASK_ID'])
except KeyError:
    print("Error: SLURM_ARRAY_TASK_ID not found")
    sys.exit(1)
except ValueError:
    print("Error: SLURM_ARRAY_TASK_ID is not an integer")
    sys.exit(1)
 
try:
    task_offset = int(os.environ.get('TASK_OFFSET', 0))
except ValueError:
    print("Error: TASK_OFFSET is not an integer")
    sys.exit(1)
 
# Global index across all batches (0-based)
instance_index = task_offset + task_id - 1


methods = ["classic"]
method_index = instance_index % len(methods)
method = methods[method_index]
print(f"Running method: {method}")

instance_index = instance_index // len(methods)

base_instances = read_instances_name("instances_name")
new_instances = read_instances_name("generated/instances_name")
all_instances = base_instances + new_instances

instance_dir = "../../instances/"

instance_name = all_instances[instance_index]

reader = InstanceReader(f"{instance_dir}{instance_name}.txt")
instance = reader.read()

solutions = {}

solutions_file = f"{method}_solutions"

if method == "classic":
    dual_optimal_solutions = {}
    vrp, solution, solution_dict = vrp_instance(instance, save=True, dir=dir, verbose=verbose)
    dual_optimal_solutions[instance_name] = solution.dual_by_var_id
    save_dict_to_json(dir, "optimal_dual", dual_optimal_solutions)
elif method == "stabilized":
    dir = f"{dir}/{method}"
    solution_dict = vrp_stabilized_instance(instance, dir=dir, verbose=verbose)
elif method == "stabilized_with_optimal_dual": 
    dir = f"{dir}/{method}"
    dual_optimal_solution = reader.read_dual_optimal(instance_name)
    solution_dict = vrp_stabilized_instance(instance, dual_box_centre=dual_optimal_solution, dir=dir, verbose=verbose)
elif method == "smoothing":
    dir = f"{dir}/{method}"
    vrp, solution, solution_dict = vrp_instance(instance, smoothing=0.8, dir=dir, verbose=verbose)
elif method == "smoothing_with_optimal_dual":
    dir = f"{dir}/{method}"
    dual_optimal_solution = reader.read_dual_optimal(instance_name)
    vrp, solution, solution_dict = vrp_instance(instance, smoothing=0.8, dual_box_centre=dual_optimal_solution, dir=dir, verbose=verbose)

solutions[instance_name] = solution_dict
save_dict_to_json(dir, solutions_file, solutions)
