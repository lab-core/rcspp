from utils.experiments import aggregate_results
from utils.definitions import DATASETS_DIR

if __name__ == "__main__":
    datasetdir = f"{DATASETS_DIR}dataset_30/"

    aggregate_results(datasetdir, "classic")
    #aggregate_results(datasetdir, "stabilized")
    #aggregate_results(datasetdir, "stabilized_with_optimal_dual")
    #aggregate_results(datasetdir, "optimal_dual")