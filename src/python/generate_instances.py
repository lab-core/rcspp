from vrp.instance import Instance
from vrp.instance_reader import InstanceReader
from utils.test_utils import *
import random

base_instances = read_instances_name("instances_name")

instance_dir = "../../instances/"
generated_instances_names = []
nR = 0
nC = 0
nRC = 0


for name in base_instances:

    instance_path = instance_dir + name + ".txt"
    reader = InstanceReader(instance_path)
    base_instance = reader.read()


    nb_customers = len(base_instance.get_demand_customers_id())
    customers_by_id = base_instance.get_customers_by_id()
    depot = base_instance.get_depot_customer()

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
    
    n = 20  # nombre de clients à sélectionner pour chaque instance générée

    tirages = []
    tirages_sets = []

    while len(tirages) < nb_tirages:
        tirage = random.sample(range(1, 100), n)
        tirage.sort()
        tirage_set = frozenset(tirage)

        # Vérifier que ce tirage est différent de tous les précédents
        if tirage_set not in tirages_sets:
            tirages.append(tirage)
            tirages_sets.append(tirage_set)
        else:
            print(f"Tirage {tirage} déjà existant, génération d'un nouveau tirage...")  
            tirage = random.sample(range(1, 100), n)
            tirage.sort()
            tirage_set = frozenset(tirage)

    for i, tirage in enumerate(tirages, 1):
        new_instance_name = f"{name}_{i}"
        new_instance = Instance(base_instance.get_nb_vehicles(), base_instance.get_capacity()//2, new_instance_name)
        new_instance.add_customer(depot.id, depot.pos_x, depot.pos_y, depot.demand, depot.ready_time, depot.due_time, depot.service_time, True)
        for i in range(len(tirage)):
            id = tirage[i]
            c= customers_by_id[id]
            new_instance.add_customer(id, c.pos_x, c.pos_y, c.demand, c.ready_time, c.due_time, c.service_time, False)

        new_instance.write_to_file(f"{instance_dir}/generated/{Itype}/{new_instance_name}.txt")
        generated_instances_names.append(new_instance_name)
    
    print(f"Generated {len(tirages)} instances for base instance {name}.")

print(f"Total generated instances: {len(generated_instances_names)}")
print(f"Number of R instances: {nR}")
print(f"Number of C instances: {nC}")
print(f"Number of RC instances: {nRC}")

with open(f"{instance_dir}/generated/instances_name.txt", "a") as f:
    for n in generated_instances_names:
        f.write(n + "\n")