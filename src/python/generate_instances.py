from vrp.instance import Instance
from vrp.instance_reader import InstanceReader
from test_utils import *
import random

base_instances = read_instances_name()

instance_dir = "../../instances/"
generated_instances_names = []


for name in base_instances:

    instance_path = instance_dir + name + ".txt"
    reader = InstanceReader(instance_path)
    base_instance = reader.read()


    nb_customers = len(base_instance.get_demand_customers_id())
    customers_by_id = base_instance.get_customers_by_id()
    depot = base_instance.get_depot_customer()

    n = 50         # nombre de client par exemple
    nb_tirages = 10

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

    for i, tirage in enumerate(tirages, 1):
        print(f"Tirage {i:2d} : {tirage}")
        new_instance_name = f"{name}_{i}"
        new_instance = Instance(base_instance.get_nb_vehicles(), base_instance.get_capacity()//2, new_instance_name)
        new_instance.add_customer(depot.id, depot.pos_x, depot.pos_y, depot.demand, depot.ready_time, depot.due_time, depot.service_time, True)
        for i in range(len(tirage)):
            id = tirage[i]
            c= customers_by_id[id]
            new_instance.add_customer(id, c.pos_x, c.pos_y, c.demand, c.ready_time, c.due_time, c.service_time, False)

        new_instance.write_to_file(f"{instance_dir}/generated/{new_instance_name}.txt")
        generated_instances_names.append(new_instance_name)

with open(f"{instance_dir}/generated/instances_name.txt", "a") as f:
    for n in generated_instances_names:
        f.write(n + "\n")