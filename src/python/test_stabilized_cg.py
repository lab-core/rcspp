#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

from vrp.instance_reader import InstanceReader
from utils.test_utils import *
from utils.definitions import INSTANCES_DIR



if __name__ == "__main__":
    dir = "Ref_stabilized_cg"
    
    print("Read instance...")
    instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]

    solutions = {}

    kappas = [i for i in range(1, 2000, 50)]

    for name in instances_name:
        instance_path = INSTANCES_DIR + name + ".txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        dual_optimal_solution = instance_reader.read_dual_optimal(instance.get_name())

        print(f"\n\n====================\n Instance: {name}\n====================\n\n")

        for p in kappas:
            print(f"\n\n====================\n Kappa: {p}\n====================\n\n")
            solution_dict = vrp_stabilized_instance(instance, dual_optimal_solution, penalty=p, kappa=p, verbose=False)
            solutions[f"{name}_{p}"] = solution_dict

    save_dict_to_json(dir, "summary_variation_kappa_epsilon", solutions)