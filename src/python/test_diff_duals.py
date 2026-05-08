from vrp.instance_reader import InstanceReader
from utils.test_utils import *
from utils.utils import *

instances_dir = "/home/jullarth/Documents/Old/instances/"

instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]

solutions = {}
duals = {}
n_cols_added = [i for i in range(20, 100, 5)]

if __name__ == "__main__":
    print("---------------------------------------------Start test_diff_duals.py")
    for name in instances_name:
        print(f"---------------------------------------------------------------Processing instance {name}...")
        instance_path = instances_dir + name + ".txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        for n_cols in n_cols_added:
            vrp = VRP(instance, verbose=False)
            solution = vrp.solve(n_cols)
            solutions[name+f"_{n_cols}"] = solution.dual_by_var_id
            duals[name+f"_{n_cols}"] = solution.dual_by_var_id

        save_dict_to_json("Number_added_column", "Study_for_each_instance", solutions)

    print("---------------------------------------------End test_diff_duals.py")

    dual_optimal = {}
    closer_duals = {}

    for name in instances_name:
        print(f"---------------------------------------------------------------Processing instance {name}...")
        instance_reader = InstanceReader("")
        dual_optimal[name] = instance_reader.read_dual_optimal(name)
        value = sum([i for i in dual_optimal[name].values()])
        diff_by_sol = {}
        
   
        for n in n_cols_added:
            diff_by_sol[name] = {}
            dual_by_id:dict[int, float] = {}
            for i in duals[f"{name}_{n}"]:
                dual_by_id[int(i)] = duals[f"{name}_{n}"][i]
            duals[f"{name}_{n}"] = dual_by_id
            print(f"Duals for {name} with {n} columns added: {dual_by_id}")
            dist = dict_l2_norm(dict_addition(dual_by_id, dict_scalar_mult(dual_optimal[name], -1)))
            diff_by_sol[name][n] = 100* dist/value

        sorted_dict = sorted(diff_by_sol[name].items(), key=lambda item: item[1])
        print(len(sorted_dict))
        closer_duals[name] = {}
        for n, value in sorted_dict[-5:]:
            closer_duals[name][n] = duals[f"{name}_{n}"]


        save_dict_to_json("Number_added_column", "Closest_duals", closer_duals)

    print("---------------------------------------------Start test_diff_duals.py - Part 2")

    solutions = {}

    for name in instances_name:
        print(f"---------------------------------------------------------------Processing instance {name}...")
        instance_path = instances_dir + name + ".txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()

        ref_dual = instance_reader.read_dual_optimal(name)
        ref_sol_dict = vrp_stabilized_instance(instance, ref_dual, verbose=False).solve()
        solutions[name] = ref_sol_dict

        for n in closer_duals[name]:
            dual = closer_duals[name][n]
            sol_dict = vrp_stabilized_instance(instance, dual, verbose=False).solve()
            solutions[name+f"_{n}"] = sol_dict

        save_dict_to_json("Number_added_column", "Solutions_for_closest_duals", solutions)
            

