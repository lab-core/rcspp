from utils.experiment_utils import vrp_stabilized_instance, vrp_instance, ensure_parent_dir, save_dict_to_json, str_dict_to_int, load_optimal_dual
from vrp.instance import Instance
import json
import zipfile
from datetime import datetime
import os
from typing import Literal


Method = Literal[
    "classic",
    "stabilized",
    "stabilized_with_optimal_dual",
    "smoothing",
    "smoothing_with_optimal_dual",
]


def _dispatch(
    instance: Instance,
    method: Method,
    dual_optimal: dict | None,
    solution_dir: str,
    optimal_dual_dir: str,
    verbose: bool,
) -> dict:
    if method == "classic":
        vrp, solution, solution_dict = vrp_instance(
            instance, save=True, dir=solution_dir, verbose=verbose
        )
        #save_dict_to_json(optimal_dual_dir, instance.get_name(), solution.dual_by_var_id)

    elif method == "stabilized":
        solution_dict = vrp_stabilized_instance(instance, dir=solution_dir, verbose=verbose)

    elif method == "stabilized_with_optimal_dual":
        solution_dict = vrp_stabilized_instance(
            instance, dual_box_centre=dual_optimal, dir=solution_dir, verbose=verbose
        )

    elif method == "smoothing":
        _, _, solution_dict = vrp_instance(
            instance, smoothing=0.8, dir=solution_dir, verbose=verbose
        )

    elif method == "smoothing_with_optimal_dual":
        _, _, solution_dict = vrp_instance(
            instance, smoothing=0.8, dual_box_centre=dual_optimal, dir=solution_dir, verbose=verbose
        )

    else:
        raise ValueError(f"Unknown method: '{method}'.")

    return solution_dict

def run_instance(instance: Instance, method: Method, dataset_dir: str, verbose: bool = True):
    instance_name = instance.get_name()
    solution_dir = f"{dataset_dir}Solutions/{method}/"
    optimal_dual_dir = f"{dataset_dir}Solutions/optimal_dual/"
    optimal_dual_path = f"{optimal_dual_dir}{instance_name}.json"

    ensure_parent_dir(solution_dir)
    ensure_parent_dir(optimal_dual_dir)

    dual_optimal = load_optimal_dual(optimal_dual_path) if "optimal" in method else None

    solution_dict = _dispatch(
        instance, method, dual_optimal, solution_dir, optimal_dual_dir, verbose
    )

    save_dict_to_json(solution_dir, instance_name, solution_dict)
    print(f"Saved solutions to {solution_dir}{instance_name}")


def baseline(instance: Instance, dataset_dir: str, verbose: bool = True):
    for method in ("classic", "stabilized", "stabilized_with_optimal_dual"):
        print(f"\n{'='*60}\n  {method}\n{'='*60}")
        run_instance(instance, method, dataset_dir, verbose)


def run_prediction_instance(instance: Instance, model_name:str, dataset_dir:str, verbose=True):
    instance_name = instance.get_name()
    prediction_dir = f"{dataset_dir}Solutions/{model_name}_predictions/"
    prediction_filename = f"{instance_name}.json"
    prediction_solution_dir = f"{dataset_dir}Solutions/{model_name}_solutions/"

    ensure_parent_dir(prediction_dir)

    dataset = ""

    if prediction_filename in os.listdir(prediction_dir + "test/"):
        dataset = "test"
    elif prediction_filename in os.listdir(prediction_dir + "train/"):
        dataset = "train"
    else:
        print(f"No predictions for this instane in the model: {model_name}")
        return False

    prediction_dir += dataset + "/"
    prediction_solution_dir += dataset + "/"

  
    with open(prediction_dir+prediction_filename, "r") as f:
            predicted_solution = json.load(f)
    
    predicted_solution = str_dict_to_int(predicted_solution[instance_name])

    solution_dict = vrp_stabilized_instance(instance, dual_box_centre=predicted_solution, dir=prediction_solution_dir, verbose=verbose)
    save_dict_to_json(prediction_solution_dir, instance_name, solution_dict)
    print(f"Saved solutions to {prediction_solution_dir}{instance_name}")

def aggregate_results(dataset_dir: str, method: str):
    dir = f"{dataset_dir}Solutions/{method}/"
    output_path = f"{dataset_dir}Solutions/{method}_solutions.json"
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    archive_dir = f"{dir}archive/"
    os.makedirs(archive_dir, exist_ok=True)

    aggregated = {}
    individual_files = []
    for fname in os.listdir(dir):
        if fname.endswith(".json"):
            instance_name = fname[:-5]
            fpath = f"{dir}{fname}"
            with open(fpath, "r") as f:
                aggregated[instance_name] = json.load(f)
            individual_files.append((fname, fpath))

    zip_path = f"{archive_dir}{method}_{timestamp}.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for fname, fpath in individual_files:
            zf.write(fpath, fname)
            #os.remove(fpath)

    with open(output_path, "w") as f:
        json.dump(aggregated, f, indent=4)