#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa
from unicodedata import name

from vrp.instance_reader import InstanceReader
from utils.definitions import DATASETS_DIR
from utils.experiment_utils import *

if __name__ == "__main__":
    datasetdir = f"{DATASETS_DIR}dataset_10_bis/"
    predictiondir = f"{datasetdir}Solutions/predictions/"
    predictions = {}
    prediction_file_name = "test_predictions"
    with open(f"{predictiondir}{prediction_file_name}.json", "r") as f:
        predictions = json.load(f)
    for key in predictions.keys():
        predictions[key] = {int(k): v for k, v in predictions[key].items()}

    print("Loaded predictions for instances:", len(list(predictions.keys())))

    verbose = True
    methods = []
    solutions = {}
    solutions_file = f"{prediction_file_name}_solutions"

    for instance_name in predictions.keys():
        print(f"Processing instance {instance_name} with predicted duals")

        if "RC" in instance_name:
            Itype = "RC" 
        elif "C" in instance_name:
            Itype = "C"
        else:   
            Itype = "R"

        reader = InstanceReader(f"{datasetdir}Instances/{Itype}/{instance_name}.txt")
        instance = reader.read()

        dual_prediction = predictions[instance_name]

        solution_dict = vrp_stabilized_instance(instance, dual_prediction, save=False, dir=f"{datasetdir}Solutions/predictions", verbose=verbose)

        solutions[instance_name] = solution_dict
        
    save_dict_to_json(f"{datasetdir}Solutions/predictions/", solutions_file, solutions)

