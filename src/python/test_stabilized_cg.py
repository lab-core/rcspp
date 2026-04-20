#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

from import_rscpp_lib import import_rscpp_lib
import_rscpp_lib()

from vrp.instance_reader import InstanceReader
from test_utils import *
from utils import add_gaussian_noise



if __name__ == "__main__":
    dir = "Multiple_instances_perturbations"
    
    print("Read instance...")
    instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]
    #instances_name = ["R101"]
    solutions = {}
    means = {}

    radius = 1.0
    penalty = 100

    for name in instances_name:
        instance_path = "../../instances/" + name + ".txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        dual_optimal_solution = instance_reader.read_dual_optimal(instance.get_name())

        print(f"\n\n====================\n Instance: {name}\n====================\n\n")

        vrp, solution, solution_dict = vrp_dual_box_instance(instance, dual_optimal_solution, radius)
        solution_dict["perturbation"] = False
        solutions[name] = solution_dict
        means[name] = solution_dict

        swp_list = []
        sp_list = []
        
        for i in range(10):
            dual_box_centre = add_gaussian_noise(dual_optimal_solution, 0.0, 0.5)

            svrp_with_penalty, solution_with_penalty, sp_dict = vrp_dual_box_instance(instance, dual_box_centre, radius, penalty)
            sp_dict["perturbation"] = True
            solutions[name+f"_{i}_with_penalty"] = sp_dict
            sp_list.append(sp_dict)
        
        means[name+"_with_penalty"] = average_dicts(sp_list)
        
        
    save_dict_to_json(dir, "summary", solutions)
    save_dict_to_json(dir, "means", means)