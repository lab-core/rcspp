#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import utils.rscpp_lib
from vrp.instance_reader import InstanceReader
from utils.definitions import *
from vrp.stabilized_vrp import StabilizedVRP
from vrp.stabilized_vrp import VRP
import json
from utils.experiment_utils import _build_solution_dict


if __name__ == "__main__":
    
    print("Read instance...")
    instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]
    instances_name = ["R202_2"]
    dual_optimal = {}
    with open("/home/jullarth/Documents/Solutions/Duaux/duaux_optimaux.json", "r") as f:
        optim = json.load(f)
    dual_optimal = {int(k):v for k, v in optim["R102"].items()}

    with open("/scratch/jullarth/datasets/dataset_30/Solutions/optimal_dual/R202_2.json", "r") as f:
        optim = json.load(f)
    dual_optimal = {int(k):v for k, v in optim.items()}
    
    verbose = True

    for name in instances_name:
        instance_path = f"/scratch/jullarth/datasets/dataset_30/Instances/R/{name}.txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        vrp = VRP(instance, verbose=verbose)
        solution = vrp.solve()
        ref_sol_dict = _build_solution_dict(vrp, solution)


        svrp = StabilizedVRP(instance, verbose=verbose)
        ssolution = svrp.solve()
        stab_sol_dict = _build_solution_dict(svrp, ssolution, {"n_meta_iter": svrp.meta_iteration, "n_center_change": svrp.nb_new_center})

        spvrp = StabilizedVRP(instance, dual_estimate=dual_optimal, verbose=verbose)
        spsolution = spvrp.solve()
        stabp_sol_dict = _build_solution_dict(spvrp, spsolution, {"n_meta_iter": spvrp.meta_iteration, "n_center_change": spvrp.nb_new_center})

        print("Solutions:")
        print(ref_sol_dict)
        print(stab_sol_dict)
        print(stabp_sol_dict)

        
