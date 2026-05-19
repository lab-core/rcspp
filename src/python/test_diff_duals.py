from vrp.instance_reader import InstanceReader
from utils.test_utils import *
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
    for n_cols in n_cols_added:
        vrp = VRP(instance, verbose=False)
        solution = vrp.solve(n_cols)
        duals[name+f"_{n_cols}"] = solution.dual_by_var_id

    save_dict_to_json(f"{SOLUTIONS_DIR}Number_added_column/", "all_duals", duals)

    print("---------------------------------------------End test_diff_duals.py")

    dual_optimal = {}
    closer_duals = {}

    with open(f"{SOLUTIONS_DIR}Duaux/duaux_optimaux.json", "r") as f:
        dual_optimal[name] = {int(k): v for k,v in json.load(f)[name].items()}

    value = sum([i for i in dual_optimal[name].values()])
    diff_by_sol = {}
    diff_by_sol[name] = {}    
   
    for n in n_cols_added:
        dual_by_id = duals[f"{name}_{n}"]
        print(f"Duals for {name} with {n} columns added")
        dist = dict_l2_norm(dict_addition(dual_by_id, dict_scalar_mult(dual_optimal[name], -1)))
        diff_by_sol[name][n] = 100* dist/value

    sorted_dict = sorted(diff_by_sol[name].items(), key=lambda item: item[1])
    closer_duals[name] = {}
    for n, value in sorted_dict[-5:]:
        closer_duals[name][n] = duals[f"{name}_{n}"]
        
    save_dict_to_json(f"{SOLUTIONS_DIR}Number_added_column/", "Closest_duals", closer_duals)

    print("---------------------------------------------Start test_diff_duals.py - Part 2")

    solutions = {}

    print(f"---------------------------------------------------------------Processing instance {name}...")

    ref_dual = dual_optimal[name]
    ref_sol_dict = vrp_stabilized_instance(instance, ref_dual, dir=f"{SOLUTIONS_DIR}solutions/", verbose=False)
    solutions[name] = ref_sol_dict

    for n in closer_duals[name]:
        dual = closer_duals[name][n]
        sol_dict = vrp_stabilized_instance(instance, dual, dir=f"{SOLUTIONS_DIR}solutions/", verbose=False)
        solutions[name+f"_{n}"] = sol_dict

    save_dict_to_json(f"{SOLUTIONS_DIR}Number_added_column/", "Solutions_for_closest_duals", solutions)
            

