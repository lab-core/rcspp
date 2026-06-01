from vrp.instance_reader import InstanceReader
from utils.experiment_utils import *
from utils.utils import *
import sys
from utils.definitions import SOLUTIONS_DIR

instances_dir = "/home/jullarth/Documents/rcspp/instances/"

instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]

solutions = {}
duals = {}
n_cols_added = [i for i in range(20, 100, 5)]

try:
    task_id = int(os.environ['SLURM_ARRAY_TASK_ID'])
except KeyError:
    print("Error: SLURM_ARRAY_TASK_ID not found")
    sys.exit(1)
except ValueError:
    print("Error: SLURM_ARRAY_TASK_ID is not an integer")
    sys.exit(1)

if __name__ == "__main__":
    name = instances_name[task_id -1]
        
    print(f"---------------------------------------------------------------Processing instance {name}...")
    instance_path = instances_dir + name + ".txt"
    instance_reader = InstanceReader(instance_path)
    instance = instance_reader.read()
    
    dual_optimal = {}
    with open(f"{SOLUTIONS_DIR}Duaux/duaux_optimaux.json", "r") as f:
        dual_optimal = json.load(f)

    closer_duals = {}
    with open(f"{SOLUTIONS_DIR}Number_added_column/Closest_duals.json", "r") as f:
        closer_duals = json.load(f)

    print("---------------------------------------------Start test_diff_duals.py - Part 2")

    solutions = {}

    print(f"---------------------------------------------------------------Processing instance {name}...")

    ref_dual = {int(k):v for k,v in dual_optimal[name].items()}
    ref_sol_dict = vrp_stabilized_instance(instance, ref_dual, dir=f"{SOLUTIONS_DIR}solutions/", verbose=False)
    solutions[name] = ref_sol_dict

    for n in closer_duals[name]:
        dual = {int(k):v for k,v in closer_duals[name][n].items()}
        sol_dict = vrp_stabilized_instance(instance, dual, dir=f"{SOLUTIONS_DIR}solutions/", verbose=False)
        solutions[name+f"_{n}"] = sol_dict

    save_dict_to_json(f"{SOLUTIONS_DIR}Number_added_column/", "Solutions_for_closest_duals", solutions)
            

