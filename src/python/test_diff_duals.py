from vrp.instance_reader import InstanceReader
from utils.test_utils import *

instances_name = ["R101", "R102", "R103", "R104", "R105", "C101", "C102", "C103", "C104", "C105", "RC101", "RC102", "RC103", "RC104", "RC105"]

solutions = {}
n_cols_added = [i for i in range(1, 100, 2)]

if __name__ == "__main__":
    for name in instances_name:
        instance_path = "../../instances/" + name + ".txt"
        instance_reader = InstanceReader(instance_path)
        instance = instance_reader.read()
        for n_cols in n_cols_added:
            vrp = VRP(instance, verbose=False)
            solution = vrp.solve(n_cols)
            solutions[name+f"_{n_cols}"] = solution.dual_by_var_id

    save_dict_to_json("Number_added_column", "Study_for_each_instance", solutions)