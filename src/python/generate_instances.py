from vrp.instance import Instance
from vrp.instance_reader import InstanceReader
from utils.test_utils import ensure_parent_dir, read_instances_name
from utils.definitions import INSTANCES_DIR, DATASETS_DIR
import random

base_instances = read_instances_name(f"{INSTANCES_DIR}instances_name.txt")

generated_instances_names = []
nR = 0
nC = 0
nRC = 0


for name in base_instances:

    instance_path = INSTANCES_DIR + name + ".txt"
    reader = InstanceReader(instance_path)
    base_instance = reader.read()

    customers_by_id = base_instance.get_customers_by_id()
    depot = base_instance.get_depot_customer()
    nb_customers = len(base_instance.get_customers_by_id())

    if "RC" in name:
        nb_tirages = 782
        Itype = "RC" 
        nRC += 1        # nombre de tirages par exemple RC
    elif "C" in name:
        nb_tirages = 736        # nombre de tirages par exemple C
        Itype = "C"
        nC += 1
    else:   
        nb_tirages = 544         # nombre de tirages par exemple R
        Itype = "R"
        nR += 1
    
    n = 30  # nombre de clients à sélectionner pour chaque instance générée

    tirages = []
    tirages_sets = []

    while len(tirages) < nb_tirages:
        tirage = random.sample(range(1, nb_customers), n)
        tirage.sort()
        tirage_set = frozenset(tirage)

        # Vérifier que ce tirage est différent de tous les précédents
        if tirage_set not in tirages_sets:
            tirages.append(tirage)
            tirages_sets.append(tirage_set)

    for i, tirage in enumerate(tirages, 1):
        new_instance_name = f"{name}_{i}"
        new_instance = Instance(base_instance.get_nb_vehicles(), base_instance.get_capacity()//5, new_instance_name)
        new_instance.add_customer(depot.id, depot.pos_x, depot.pos_y, depot.demand, depot.ready_time, depot.due_time, depot.service_time, True)
        for idx, customer_id in enumerate(tirage):
            c = customers_by_id[customer_id]
            new_instance.add_customer(idx+1, c.pos_x, c.pos_y, c.demand, c.ready_time, c.due_time, c.service_time, False)

        ensure_parent_dir(f"{DATASETS_DIR}dataset_{n}/Instances/{Itype}/")
        new_instance.write_to_file(f"{DATASETS_DIR}dataset_{n}/Instances/{Itype}/{new_instance_name}.txt")
        generated_instances_names.append(new_instance_name)
    
    print(f"Generated {len(tirages)} instances for base instance {name}.")

print(f"Total generated instances: {len(generated_instances_names)}")
print(f"Number of R instances: {nR}")
print(f"Number of C instances: {nC}")
print(f"Number of RC instances: {nRC}")

ensure_parent_dir(f"{DATASETS_DIR}dataset_{n}/Instances/")
with open(f"{DATASETS_DIR}dataset_{n}/Instances/instances_name.txt", "a") as f:
    for name in generated_instances_names:
        f.write(name + "\n")