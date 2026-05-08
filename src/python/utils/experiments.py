from utils.test_utils import *
from vrp.instance import Instance
import json
import zipfile
from datetime import datetime

def baseline(instance: Instance, dataset_dir:str, verbose=True):
    print("--------------------------------------------Classic-------------------------------------")
    run_instance(instance, "classic", dataset_dir, verbose)
    print("\n \n \n")
    print("--------------------------------------------Stabilized-------------------------------------")
    run_instance(instance, "stabilized", dataset_dir, verbose)
    print("\n \n \n")
    print("--------------------------------------------Stabilized with dual optimal-------------------------------------")
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
    optimal_dual_dir = f"{dataset_dir}Solutions/optimal_dual/"
    optimal_dual_path = f"{optimal_dual_dir}{instance_name}.json"

    ensure_parent_dir(dir)
    ensure_parent_dir(optimal_dual_dir)

    if "optimal" in method:
        assert os.path.exists(optimal_dual_path), (
            f"Dual optimal manquant pour '{instance_name}'. "
            f"La méthode 'classic' a-t-elle bien été exécutée avant ?"
        )
        with open(optimal_dual_path, "r") as f:
            dual_optimal_solution = str_dict_to_int(json.load(f))

    if method == "classic":
        vrp, solution, solution_dict = vrp_instance(instance, save=True, dir=dir, verbose=verbose)
        save_dict_to_json(optimal_dual_dir, instance_name, solution.dual_by_var_id)
        print(f"Saving dual optimal to {optimal_dual_path}")

    elif method == "stabilized":
        solution_dict = vrp_stabilized_instance(instance, dir=dir, verbose=verbose)
    elif method == "stabilized_with_optimal_dual":
        solution_dict = vrp_stabilized_instance(instance, dual_box_centre=dual_optimal_solution, dir=dir, verbose=verbose)
    elif method == "smoothing":
        vrp, solution, solution_dict = vrp_instance(instance, smoothing=0.8, dir=dir, verbose=verbose)
    elif method == "smoothing_with_optimal_dual":
        vrp, solution, solution_dict = vrp_instance(instance, smoothing=0.8, dual_box_centre=dual_optimal_solution, dir=dir, verbose=verbose)

    save_dict_to_json(dir, instance_name, solution_dict)
    print(f"Saved solutions to {dir}{instance_name}")

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
        return True

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