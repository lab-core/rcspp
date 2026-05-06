from utils.test_utils import *
from vrp.instance import Instance
import json

def baseline(instance: Instance, dataset_dir:str, verbose=True):
    run_instance(instance, "classic", dataset_dir, verbose)
    run_instance(instance, "stabilized", dataset_dir, verbose)
    run_instance(instance, "stabilized_with_optimal_dual", dataset_dir, verbose)


def run_instance(instance: Instance, method:str, dataset_dir:str, verbose=True):
    """
    Possible methods:
    - classic : Column Generation
    - stabilized : Stabilized Column Generation (SCG) as presented in Pessoa et al. 2018
    - stabilized_with_optimal_dual : SCG with dual optimal solution
    - smoothing : Smoothing as presented in Pessoa et al. 2018
    - smoothing_with_optimal_dual : Smoothing with start at dual optimal solution
    """
    instance_name = instance.get_name()
    dir = f"{dataset_dir}Solutions/{method}/"
    solutions_file = f"{method}_solutions"

    ensure_parent_dir(dir)

    if "optimal" in method:
        all_optimal_dual = {}
        with open(f"{dataset_dir}Solutions/optimal_dual.json", "r") as f:
            all_optimal_dual = json.load(f)
        assert instance_name in all_optimal_dual

        dual_optimal_solution = str_dict_to_int(all_optimal_dual[instance_name])

    if method == "classic":
        dual_optimal_solutions = {}
        vrp, solution, solution_dict = vrp_instance(instance, save=True, dir=dir, verbose=verbose)
        dual_optimal_solutions[instance_name] = solution.dual_by_var_id
        save_dict_to_json(f"{dataset_dir}Solutions/", "optimal_dual", dual_optimal_solutions)
    elif method == "stabilized":
        solution_dict = vrp_stabilized_instance(instance, dir=dir, verbose=verbose)
    elif method == "stabilized_with_optimal_dual": 
        solution_dict = vrp_stabilized_instance(instance, dual_box_centre=dual_optimal_solution, dir=dir, verbose=verbose)
    elif method == "smoothing":
        vrp, solution, solution_dict = vrp_instance(instance, smoothing=0.8, dir=dir, verbose=verbose)
    elif method == "smoothing_with_optimal_dual":
        dir = f"{dir}/{method}"
        vrp, solution, solution_dict = vrp_instance(instance, smoothing=0.8, dual_box_centre=dual_optimal_solution, dir=dir, verbose=verbose)

    save_dict_to_json(dir, solutions_file, {instance_name: solution_dict})