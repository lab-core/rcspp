#!/usr/bin/env python3

import os
import sys
from vrp.instance_reader import InstanceReader
from utils.definitions import DATASETS_DIR
from utils.experiments import run_prediction_instance
from utils.test_utils import read_instances_name

dataset_dir = f"{DATASETS_DIR}dataset_30/"
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

try:
    task_number = int(os.environ.get('TASK_NUMBER', 0))
except ValueError:
    print("Error: TASK_NUMBER is not an integer")
    sys.exit(1)


print(f"######### TASK_NUMBER: {task_number}")
 
# Global index across all batches (0-based)
instance_index = task_offset + task_number*(task_id - 1)

all_instances = read_instances_name(f"{dataset_dir}Instances/instances_name.txt")


for i in range(task_number):

    instance_name = all_instances[instance_index]

    print(f"##########################################################{instance_name}#####################")

    if "RC" in instance_name:
        Itype = "RC"
    elif "C" in instance_name:
        Itype = "C"
    else:
        Itype = "R"

    reader = InstanceReader(f"{dataset_dir}Instances/{Itype}/{instance_name}.txt")
    instance = reader.read()
    run_prediction_instance(instance, "MLP-all", dataset_dir)

    instance_index += 1
   
