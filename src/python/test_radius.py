#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import utils.rscpp_lib
from vrp.instance_reader import InstanceReader
from utils.definitions import *
from vrp.stabilized_vrp import StabilizedVRP
from vrp.stabilized_vrp import VRP
import json
from utils.experiments import baseline


if __name__ == "__main__":
    
    print("Read instance...")
    instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]
    instances_name = ["C102"]
    dual_optimal = {}
    with open("/home/jullarth/Documents/Solutions/Duaux/duaux_optimaux.json", "r") as f:
        optim = json.load(f)
    dual_optimal = {int(k):v for k, v in optim["C102"].items()}
    
    
    verbose = True

    for name in instances_name:
        instance_path = f"/scratch/jullarth/datasets/dataset_30/Instances/R/{name}.txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        
        baseline(instance, "")

        
