from vrp.dual_box_vrp import DualBoxVRP
from vrp.vrp import VRP
from solution_formatter import format_solution
import json

def vrp_dual_box_instance(instance, dual_box_centre, radius, penalty=None, save=False, dir="", verbose=True):
    print("Construct VRP")
    vrp = DualBoxVRP(instance, dual_box_centre, radius, penalty, verbose=verbose)
    print("Construct VRP ...Done")

    solution = vrp.solve()

    formatted_solution = format_solution(
                solution,
                vrp._VRP__paths,
                vrp.depot_id_,
                instance_name=instance.get_name(),
                author="arthus",
            )
    if save:
        with open(f"/home/jullarth/Documents/Solutions/{dir}/{instance.get_name()}.txt", "w") as f:
           f.write(formatted_solution)

    solution_dict = {"radius": radius, 
                        "n_iter": vrp.get_n_iterations(), 
                        "total_time": vrp.get_total_problem_time(),
                        "lp_cost": vrp.get_lp_cost(),
                        "solution_cost": solution.cost,
                        "time_ratio": vrp.get_total_subproblem_time()/vrp.get_total_problem_time(),
                        "nb_added_columns": len(vrp._VRP__paths),
                        "penalty": (penalty if penalty is not None else False),
                        "n_meta_iter": vrp.meta_iteration
                        }

    return vrp, solution, solution_dict

def vrp_instance(instance, smoothing=None, smoothing_center=None, save=False, dir="", verbose=True):
    print("Construct VRP")
    vrp = VRP(instance, verbose=verbose)
    print("Construct VRP ...Done")

    if smoothing is not None:
        vrp.enable_smoothing(smoothing_center, smoothing)

    solution = vrp.solve()

    formatted_solution = format_solution(
                solution,
                vrp._VRP__paths,
                vrp.depot_id_,
                instance_name=instance.get_name(),
                author="arthus",
            )
    if save:
        with open(f"/home/jullarth/Documents/Solutions/{dir}/{instance.get_name()}.txt", "w") as f:
           f.write(formatted_solution)

    solution_dict = {"smoothing": smoothing, 
                        "n_iter": vrp.get_n_iterations(), 
                        "total_time": vrp.get_total_problem_time(),
                        "lp_cost": vrp.get_lp_cost(),
                        "solution_cost": solution.cost,
                        "time_ratio": vrp.get_total_subproblem_time()/vrp.get_total_problem_time(),
                        "nb_added_columns": len(vrp._VRP__paths)
                        }

    return vrp, solution, solution_dict


def average_dicts(dict_list):
    """
    Calcule la moyenne des valeurs numériques pour chaque clé dans une liste de dictionnaires.
    Ignore les clés avec des valeurs booléennes.
    """
    if not dict_list:
        return {}

    all_keys = set()
    for d in dict_list:
        all_keys.update(d.keys())
    
    averages = {}
    for key in all_keys:
        values = []
        for d in dict_list:
            if key in d:
                val = d[key]
                if isinstance(val, bool):
                    break
                elif isinstance(val, (int, float)):
                    values.append(val)
        else:
            if values:
                averages[key] = sum(values) / len(values)
    
    return averages

def save_dict_to_json(dir:str, filename:str, d:dict):
    with open(f"../../../Solutions/{dir}/{filename}.json", "w") as f:
        json.dump(d, f, indent=4)