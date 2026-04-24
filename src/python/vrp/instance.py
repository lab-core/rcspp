#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from typing import Optional


class Customer:
    def __init__(
        self,
        id: int,
        pos_x: float,
        pos_y: float,
        demand: int,
        ready_time: int,
        due_time: int,
        service_time: int,
        depot: bool,
    ):
        self.id = id
        self.pos_x = pos_x
        self.pos_y = pos_y
        self.demand = demand
        self.ready_time = ready_time
        self.due_time = due_time
        self.service_time = service_time
        self.depot = depot


class Instance:
    def __init__(self, nb_vehicles: int, capacity: int, name: Optional[str] = None):
        self.__nb_vehicles = nb_vehicles
        self.__capacity = capacity
        self.__name = name
        self.__depot_customer_id: int = 0
        self.__customers_by_id: dict[int, Customer] = {}
        self.__demand_customers_id: list[int] = []

    def add_customer(
        self,
        id: int,
        pos_x: float,
        pos_y: float,
        demand: int,
        ready_time: int,
        due_time: int,
        service_time: int,
        depot: bool,
    ) -> Customer:
        # print("add_customer")
        # print(id, pos_x, pos_y, demand, ready_time, due_time, service_time, depot)

        if depot:
            self.__depot_customer_id = id
        else:
            self.__demand_customers_id.append(id)

        self.__customers_by_id[id] = Customer(
            id, pos_x, pos_y, demand, ready_time, due_time, service_time, depot
        )
        return self.__customers_by_id[id]

    def get_customers_by_id(self) -> dict[int, Customer]:
        return self.__customers_by_id

    def get_customer(self, id: int) -> Customer:
        return self.__customers_by_id[id]

    def get_depot_customer(self) -> Customer:
        return self.__customers_by_id[self.__depot_customer_id]

    def get_demand_customers_id(self) -> list[int]:
        return self.__demand_customers_id

    def get_nb_vehicles(self) -> int:
        return self.__nb_vehicles

    def get_capacity(self) -> int:
        return self.__capacity
    
    def get_name(self) -> str:
        return self.__name
    
    def write_to_file(self, filepath: str) -> None:
        """Écrit l'instance dans un fichier au format Solomon."""
        with open(filepath, "w") as f:
            # Nom de l'instance
            f.write(f"{self.__name or 'INSTANCE'}\n\n")

            # Section véhicules
            f.write("VEHICLE\n")
            f.write(f"NUMBER     CAPACITY\n")
            f.write(f"{self.__nb_vehicles:>8}{self.__capacity:>11}\n\n")

            # Section clients
            f.write("CUSTOMER\n")
            f.write(
                f"{'CUST NO.':>8}{'XCOORD.':>10}{'YCOORD.':>10}"
                f"{'DEMAND':>10}{'READY TIME':>12}{'DUE DATE':>10}{'SERVICE':>10}{'TIME':>7}\n"
            )
            f.write(" \n")

            # Dépôt en premier, puis les clients
            depot = self.get_depot_customer()
            all_ids = [depot.id] + self.__demand_customers_id

            for cid in all_ids:
                c = self.__customers_by_id[cid]
                f.write(
                    f"{c.id:>5}"
                    f"{int(c.pos_x):>9}"
                    f"{int(c.pos_y):>11}"
                    f"{c.demand:>11}"
                    f"{c.ready_time:>12}"
                    f"{c.due_time:>11}"
                    f"{c.service_time:>11}"
                    f"   \n"
                )
