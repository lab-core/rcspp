import numpy as np
import math
from vrp.instance import Customer

def add_gaussian_noise(vector: dict[int, float], mean: float = 0.0, std: float = 1.0) -> dict[int, float]:
    return {key: value + np.random.normal(mean, std) for key, value in vector.items()}

def multiplicative_gaussian_noise(vector: dict[int, float], mean: float = 1, std: float = 0.1) -> dict[int, float]:
    return {key: value * np.random.normal(mean, std) for key, value in vector.items()}

def dict_l1_norm(vector: dict[int, float]):
    return sum(abs(v) for v in vector.values())

def dict_l2_norm(vector: dict[int, float]):
    return math.sqrt(sum(v**2 for v in vector.values()))

def dict_dot_product(vector1:dict[int, float], vector2:dict[int, float]) -> float:
    if vector1.keys() != vector2.keys():
        raise ValueError("Dicts must have the same keys")
    return sum(vector1[k] * vector2[k] for k in vector1)

def dict_addition(vector1:dict[int, float], vector2:dict[int, float]) -> dict:
    if vector1.keys() != vector2.keys():
            raise ValueError("Dicts must have the same keys")
    return {k: vector1[k] + vector2[k] for k in vector1}

def dict_scalar_mult(vector:dict[int, float], multiplier:float) -> dict:
    return {k: multiplier * v for k, v in vector.items()}

def calculate_distance(customer1: Customer, customer2: Customer) -> float:
    return math.sqrt(
            (customer2.pos_x - customer1.pos_x) ** 2 + (customer2.pos_y - customer1.pos_y) ** 2
        )
