from math import floor, ceil

from utils.utils import calculate_distance
from vrp.instance import Instance

def time_window_reduction(instance) -> Instance:
    """
    Réduit les fenêtres temporelles de l'instance selon la méthode présentée dans A New Optimization Algorithm for the Vehicle Routing Problem with Time
Windows
    """
    customers = instance.get_customers_by_id()
    change = True

    while change:
        change = False
        for customer in customers.values():
            a = customer.ready_time
            b = customer.due_time

            min_arrival_time_from_predecessor = min([pred.ready_time + calculate_distance(pred, customer) for pred in customers.values() if pred.id != customer.id])
            min_arrival_time_to_successor = min([succ.ready_time - calculate_distance(customer, succ) for succ in customers.values() if succ.id != customer.id])
            max_departure_time_from_predecessor = max([pred.due_time + calculate_distance(pred, customer) for pred in customers.values() if pred.id != customer.id])
            max_departure_time_to_successor = max([succ.due_time - calculate_distance(customer, succ) for succ in customers.values() if succ.id != customer.id])

            if a < min(b, min_arrival_time_from_predecessor, min_arrival_time_to_successor):
                change = True
                print(f"Réduction de la fenêtre temporelle du client {customer.id} : [{a}, {b}] -> [{min(b, min_arrival_time_from_predecessor, min_arrival_time_to_successor)}, {b}]")
            if b > max(a, max_departure_time_from_predecessor, max_departure_time_to_successor):
                change = True
                print(f"Réduction de la fenêtre temporelle du client {customer.id} : [{a}, {b}] -> [{a}, {max(a, max_departure_time_from_predecessor, max_departure_time_to_successor)}]")

            a = max(a, min(b, min_arrival_time_from_predecessor, min_arrival_time_to_successor))
            b = min(b, max(a, max_departure_time_from_predecessor, max_departure_time_to_successor))
        
            customer.ready_time = a
            customer.due_time = b

    for customer in customers.values():
        customer.ready_time = floor(customer.ready_time)
        customer.due_time = ceil(customer.due_time)

    return instance