#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

from vrp.instance_reader import InstanceReader
from utils.test_utils import *
from utils.definitions import INSTANCES_DIR


dir = "all_solutions"
verbose = True


#base_instances = read_instances_name("instances_name")
new_instances = read_instances_name("generated/instances_name")
#all_instances = base_instances + new_instances
all_instances = new_instances

for instance_name in all_instances:

    reader = InstanceReader(f"{INSTANCES_DIR}{instance_name}.txt")
    instance = reader.read()

    solutions = {}

    solutions_file = f"classic_solutions"

    dual_optimal_solutions = {}
    vrp, solution, solution_dict = vrp_instance(instance, save=True, dir=dir, verbose=verbose)
    dual_optimal_solutions[instance_name] = solution.dual_by_var_id
    save_dict_to_json(dir, "optimal_dual", dual_optimal_solutions)

    solutions[instance_name] = solution_dict
    save_dict_to_json(dir, solutions_file, solutions)
