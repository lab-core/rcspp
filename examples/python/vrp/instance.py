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
