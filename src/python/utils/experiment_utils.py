import utils.rscpp_lib
from vrp.stabilized_vrp import StabilizedVRP
from vrp.vrp import VRP
from utils.solution_formatter import format_solution
import json
import os
from vrp.instance import Instance
from typing import Callable

def ensure_parent_dir(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)

def _save_formatted_solution(vrp, solution, instance, save, dir):
    formatted_solution = format_solution(
        solution,
        vrp._paths,
        vrp.depot_id_,
        instance_name=instance.get_name(),
        author="ajdepommerol",
    )
    if save:
        file_path = f"{dir}solutions/{instance.get_name()}.txt"
        ensure_parent_dir(file_path)
        with open(file_path, "w") as f:
            f.write(formatted_solution)


def _dump_dual_history(vrp, instance, dir):
    dual_history = vrp.get_dual_values_history()
    file_path = f"{dir}/dual_history/{instance.get_name()}.json"
    ensure_parent_dir(file_path)
    with open(file_path, "w") as f:
        json.dump(dual_history, f)


def _build_solution_dict(vrp, solution, extra_fields=None):
    solution_dict = {
        "n_iter": vrp.get_n_iterations(),
        "total_time": vrp.get_total_problem_time(),
        "lp_cost": vrp.get_lp_cost(),
        "solution_cost": solution.cost,
        "time_ratio": vrp.get_total_subproblem_time() / vrp.get_total_problem_time(),
        "nb_added_columns": len(vrp._paths),
    }
    if extra_fields:
        solution_dict.update(extra_fields)
    return solution_dict


def vrp_stabilized_instance(instance: Instance, dual_box_centre=None, save=False, dir="", verbose=True):
    print("Construct VRP")
    vrp = StabilizedVRP(instance, dual_box_centre, verbose=verbose)
    print("Construct VRP ...Done")

    solution = vrp.solve()
    _save_formatted_solution(vrp, solution, instance, save, dir)
    solution_dict = _build_solution_dict(vrp, solution, {"n_meta_iter": vrp.meta_iteration,  "n_center_change": vrp.nb_new_center})
    _dump_dual_history(vrp, instance, dir)

    return solution_dict


def vrp_instance(instance: Instance, smoothing=None, save=False, dir="", verbose=True):
    print("Construct VRP")
    vrp = VRP(instance, verbose=verbose)
    print("Construct VRP ...Done")

    if smoothing is not None:
        vrp.enable_smoothing(smoothing)

    solution = vrp.solve()
    _save_formatted_solution(vrp, solution, instance, save, dir)
    solution_dict = _build_solution_dict(vrp, solution)
    _dump_dual_history(vrp, instance, dir)

    return vrp, solution, solution_dict


def _aggregate_dicts(dict_list: list[dict], agg_fn: Callable, initial) -> dict:
    if not dict_list:
        return {}
    all_keys = set().union(*dict_list)
    result = {}
    for key in all_keys:
        values = []
        for d in dict_list:
            val = d.get(key)
            if val is None or isinstance(val, bool):
                break
            if isinstance(val, (int, float)):
                values.append(val)
        else:
            if values:
                result[key] = agg_fn(values)
    return result

def average_dicts(dict_list): return _aggregate_dicts(dict_list, lambda v: sum(v)/len(v), None)
def max_dicts(dict_list):     return _aggregate_dicts(dict_list, max, None)
def min_dicts(dict_list):     return _aggregate_dicts(dict_list, min, None)

def save_dict_to_json(dir: str, filename: str, d: dict):
    file_path = f"{dir}{filename}.json"
    existing_data = {}
    try:
        with open(file_path, "r") as f:
            existing_data = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        pass
        
    existing_data.update(d)
        
    with open(file_path, "w") as f:
        json.dump(existing_data, f, indent=4)

def read_instances_name(filepath:str) -> list[str]:
    names = []

    with open(filepath, "r") as f:
        for line in f:
            names.append(line.strip())

    return names

def str_dict_to_int(d: dict):
    return {int(key):value for key, value in d.items()}

def load_optimal_dual(path: str) -> dict:
    assert os.path.exists(path), (
        f"Dual optimal manquant : '{path}'. "
        f"La méthode 'classic' a-t-elle bien été exécutée avant ?"
    )
    with open(path, "r") as f:
        return str_dict_to_int(json.load(f))