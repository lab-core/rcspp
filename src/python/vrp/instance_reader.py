#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from vrp.instance import Instance


class InstanceReader:
    def __init__(self, file_path: str):
        self.file_path_ = file_path

    def read(self) -> Instance:
        print("InstanceReader::read()")
        nb_vehicles = 0
        capacity = 0

        print(f"file_path_={self.file_path_}")

        with open(self.file_path_, "r") as f:
            # First line contains the name of the instance
            instance_name = f.readline().strip()

            # Skip lines 2 to 4
            f.readline()
            f.readline()
            f.readline()

            # Line 5 contains the number of vehicles and the vehicle capacity
            nb_vehicles, capacity = map(int, f.readline().split())

            instance = Instance(nb_vehicles, capacity, instance_name)

            # Skip lines 6 to 9
            f.readline()
            f.readline()
            f.readline()
            f.readline()

            # The remaining lines contain information about customers
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue

                id = int(parts[0])
                pos_x = float(parts[1])
                pos_y = float(parts[2])
                demand = int(parts[3])
                ready_time = int(parts[4])
                due_time = int(parts[5])
                service_time = int(parts[6])

                depot = id == 0
                instance.add_customer(
                    id, pos_x, pos_y, demand, ready_time, due_time, service_time, depot
                )

        print(f"nb_customers: {len(instance.get_customers_by_id())}")
        return instance

    def read_duals(self, duals_file_path: str) -> dict[int, float]:
        print("read_duals")
        dual_by_var_id: dict[int, float] = {}

        with open(duals_file_path, "r") as f:
            for line in f:
                parts = line.strip().split()
                if not parts:
                    continue
                id = int(parts[0])
                dual_value = float(parts[1])
                dual_by_var_id[id] = dual_value

        return dual_by_var_id
